#include "stream.h"

/***********************
 ** PRIVATE FUNCTIONS **
 ***********************/

static void _onCreated (redisAsyncContext *c, void *r, void *priv);

static void _onMessage (redisAsyncContext *c, void *r, void *priv);

static void _onFreeMe (redisAsyncContext *c, void *r, void *priv);

static void _onFreeUs (redisAsyncContext *c, void *r, void *priv);

static void _onTimeout (int fd, short ev, void *priv);

static char* _super_print(const char *fmt, ...);


/********************
 ** IMPLEMENTATION **
 ********************/

/*
 * Constructor
 */

Stream_t* Stream_create (redisAsyncContext *c,
                         redisAsyncContext *subs,
                         char *id,
                         void (*callback)(Stream_t *))
{
    Stream_t *s;
    char *message;

    s = (Stream_t*) malloc(sizeof(Stream_t));
    if (s == NULL) return NULL;

    s->frameLength = 1;
    s->frameRate = 1;
    s->dimensions = 1;
    s->id = strdup(id);
    s->redisContext = c;
    s->onCreated = callback;
    s->onUpdated = NULL;
    s->onPolled = NULL;
    s->interval.tv_sec = 1;
    s->interval.tv_usec = 0;
    s->priv = NULL;

    redisAsyncCommand(c, _onFreeMe, NULL, "MULTI");
    redisAsyncCommand(c, _onFreeMe, NULL, "SADD stream %s", s->id);
    redisAsyncCommand(c, _onFreeMe, NULL,
        "HMSET stream:%s frameLength %d frameRate %d dimensions %d",
        s->id, s->frameLength, s->frameRate, s->dimensions
    );
    message = _super_print("{"
        "\"frameLength\": %d,"
        "\"frameRate\": %d,"
        "\"dimensions\": %d"
    "}", s->frameLength, s->frameRate, s->dimensions);
    Stream_publishEvent(s, "create", message);
    free(message);
    redisAsyncCommand(c, _onCreated, s, "EXEC");

    redisAsyncCommand(subs, _onMessage, s, "SUBSCRIBE stream:%s:feed", s->id);
    return s;
}


/*
 * Main methods
 */

int Stream_update (Stream_t *s,
                   char *field,
                   int value)
{
    char *message;

    if (strcmp(field, "frameLength") == 0) {
        s->frameLength = value;
    } else if (strcmp(field, "frameRate") == 0) {
        s->frameRate = value;
        double interval = 1.0 / value;
        time_t sec = (time_t) floor(interval);
        suseconds_t usec = (suseconds_t) floor((interval-sec) * 1e6);
        s->interval.tv_sec = sec;
        s->interval.tv_usec = usec;
    } else if (strcmp(field, "dimensions") == 0) {
        s->dimensions = value;
    } else return 1;

    redisAsyncCommand(s->redisContext, _onFreeMe, NULL,
        "HSET stream:%s %s %d",
        s->id, field, value
    );

    message = _super_print("{\"%s\":%d}", field, value);
    Stream_publishEvent(s, "update", message);
    free(message);

    return 0;
}


int Stream_sendFrame (Stream_t *s,
                      char *data,
                      size_t length)
{
    redisAsyncCommand(s->redisContext, _onFreeMe, NULL,
        "PUBLISH stream:%s:pipe %b",
        s->id, data, length
    );
    free(data);

    return 0;
}


int Stream_startPolling (Stream_t *s)
{
    event_set(&(s->timer), -1, EV_TIMEOUT , _onTimeout, s);
    event_add(&(s->timer), &(s->interval));
    return 0;
}


int Stream_stopPolling (Stream_t *s)
{
    event_del(&(s->timer));
    return 0;
}


/*
 * Helpers
 */

int Stream_publishEvent (Stream_t *s,
                         char* method,
                         char* data)
{
    char *message;
    message = _super_print("{"
        "\"clientID\": \""CLIENT_ID"\","
        "\"method\": \"%s\","
        "\"data\": %s"
    "}"
    , method, data);

    redisAsyncCommand(s->redisContext, _onFreeMe, NULL,
        "PUBLISH stream:%s:feed %s",
        s->id, message
    );

    if (message) free(message);

    return 0;
}


/*
 * Callbacks
 */

void _onCreated (redisAsyncContext *c,
                 void *r,
                 void *priv)
{
    Stream_t *s = (Stream_t*) priv;

    if (s->onCreated != NULL) {
        s->onCreated(s);
    }
}


void _onMessage (redisAsyncContext *c,
                 void *r,
                 void *priv)
{
    Stream_t *s  = (Stream_t*) priv;
    redisReply *reply = (redisReply*) r;
    int i = 0;
    json_value *data = NULL;
    bool updating = false;
    bool valid = true;

    if (reply == NULL) return;

    printf("Hey, we got a message!\n");

    if (reply->type == REDIS_REPLY_ARRAY &&
        reply->elements == 3 &&
        strcmp(reply->element[0]->str, "subscribe") != 0 &&
        reply->element[2]->type == REDIS_REPLY_STRING) {

            printf("Seems legit...\n");

            json_value *root = json_parse(
                reply->element[2]->str,
                reply->element[2]->len
            );

            if (root == NULL) {
                printf("Oops! Malformed JSON sequence.\n");
                json_value_free(root);
                return;
            }

            for (i = 0; i < root->u.object.length; ++i) {
                json_char *name = root->u.object.values[i].name;
                json_value *value = root->u.object.values[i].value;
                if (strcmp(name, CLIENT_ID) == 0) {
                    printf("Ouch. It was mine.\n");
                    valid = false;
                    break;
                } else if (strcmp(name, "data") == 0) {
                    data = value;
                } else if (strcmp(name, "method") == 0 &&
                           strcmp(value->u.string.ptr, "update") == 0) {
                    printf("And it's an update. Good.\n");
                    updating = true;
                }
            }

            if (valid && updating && data != NULL) {
                printf("Cool! Let's decode it.\n");
                for (i = 0; i < data->u.object.length; ++i) {
                    json_char *name = data->u.object.values[i].name;
                    json_value *value = data->u.object.values[i].value;

                    if (strcmp(name, "frameLength") == 0) {
                        s->frameLength = (int) value->u.integer;
                        printf("Updated frameLength: %d\n", s->frameLength);
                    } else if (strcmp(name, "frameRate") == 0) {
                        int rate = (int) value->u.integer;
                        double interval = 1.0 / rate;
                        time_t sec = (time_t) floor(interval);
                        suseconds_t usec = (suseconds_t) floor((interval-sec) * 1e6);
                        s->frameRate = rate;
                        s->interval.tv_sec = sec;
                        s->interval.tv_usec = usec;
                        printf("Updated frameRate: %d\n", s->frameRate);
                    } else if (strcmp(name, "dimensions") == 0) {
                        s->dimensions = (int) value->u.integer;
                        printf("Updated dimensions: %d\n", s->dimensions);
                    }

                    if (s->onUpdated != NULL) {
                        s->onUpdated(s);
                    }
                }
            }

            json_value_free(root);

    } else {

        printf("Ouch. Reply not valid:\n");
        int i;
        for (i = 0; i < reply->elements; i++) {
            printf("    %s\n", reply->element[i]->str);
        }

    }
}


void _onFreeMe (redisAsyncContext *c,
                void *r,
                void *priv)
{
    if (priv) free(priv);
}


void _onFreeUs (redisAsyncContext *c,
                void *r,
                void *priv)
{
    // THIS IS DANGEROUS!! PUT A NULL POINTER AT THE END!!
    if (!priv) return;
    void** pts = (void**) priv;
    while (*pts) {
        free(*pts);
        pts++;
    }
}


void _onTimeout (int fd,
                 short ev,
                 void* priv)
{
    // TODO: check if onPolled takes longer than the interval
    Stream_t *s = (Stream_t*) priv;

    event_add(&(s->timer), &(s->interval));
    if (s->onPolled != NULL) {
        s->onPolled(s);
    }
}


/*
 * Utils
 */

// sprintf-like with dynamic allocation
char* _super_print (const char *fmt, ...)
{
    va_list args_init;
    va_list args;
    va_start(args_init, fmt);
    va_copy(args, args_init);
    size_t len = vsnprintf(NULL, 0, fmt, args_init) + 1;
    char *strp = (char*) malloc(len);
    if (strp == NULL) return NULL;
    vsnprintf(strp, len, fmt, args);
    va_end(args_init);
    va_end(args);
    return strp;
}
