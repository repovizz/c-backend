#include "entity.h"

using namespace std;

/***********************
 ** PRIVATE FUNCTIONS **
 ***********************/

static void _onCreated (redisAsyncContext *c, void *r, void *priv);

static void _onMessage (redisAsyncContext *c, void *r, void *priv);

static void _onFreeMe (redisAsyncContext *c, void *r, void *priv);

static void _onFreeUs (redisAsyncContext *c, void *r, void *priv);

static void _onTimeout (int fd, short ev, void *priv);

static char* _super_print(const char *fmt, ...);

static string stringify (attr_value v);
static void parse (attr_value *v, string s);


/********************
 ** IMPLEMENTATION **
 ********************/

/*
 * Constructor
 */

Entity::Entity (redisAsyncContext *c,
                redisAsyncContext *subs,
                char *type,
                char *id,
                void (*callback)(Entity *),
                attr_map *init_attrs)
{
    char *message;

    if (init_attrs != NULL)
        this->attrs = *init_attrs;
    else
        this->attrs = new attribute_map();
    this->changedAttrs = new attribute_map(this->attrs);
    this->type = strdup(type);
    this->id = strdup(id);
    this->redisContext = c;
    this->onCreated = callback;
    this->onUpdated = NULL;
    this->onPolled = NULL;
    this->interval.tv_sec = 1;
    this->interval.tv_usec = 0;
    this->priv = NULL;

    redisAsyncCommand(c, _onFreeMe, NULL, "MULTI");
    redisAsyncCommand(c, _onFreeMe, NULL, "SADD %s %s", this->type, this->id);

    attr_map at = this->attrs;

    if (! at.empty()) {
        for (attr_map::iterator it = at.begin(); it != at.end(); it++) {

        }

    }
    redisAsyncCommand(c, _onFreeMe, NULL,
        "HMSET stream:%s frameLength %d frameRate %d dimensions %d",
        stringify(this->id), this->frameLength, this->frameRate, this->dimensions
    );
    }

    message = _super_print("{"
        "\"frameLength\": %s,"
        "\"frameRate\": %s,"
        "\"dimensions\": %s"
    "}", stringify(this->frameLength), stringify(this->frameRate), stringify(this->dimensions));
    publishEvent("create", message);
    free(message);
    redisAsyncCommand(c, _onCreated, this, "EXEC");

    redisAsyncCommand(subs, _onMessage, this, "SUBSCRIBE stream:%s:feed", this->id);
}


/*
 * Main methods
 */


int Entity::update (string field,
                    attr_value value)
{
    char *message;

    this->attrs[field] = value;

    }

    redisAsyncCommand(this->redisContext, _onFreeMe, NULL,
        "HSET %s:%s %s %d",
        this->type, this->id, field.c_str(), value
    );

    message = _super_print("{\"%s\":%d}", field.c_str(), value);
    Entity_publishEvent(s, "update", message);
    free(message);

    return 0;
}


int Entity::startPolling ()
{
    event_set(&(this->timer), -1, EV_TIMEOUT , _onTimeout, this);
    event_add(&(this->timer), &(this->interval));
    return 0;
}


int Entity::stopPolling ()
{
    event_del(&(this->timer));
    return 0;
}


/*
 * Helpers
 */

int Entity::publishEvent (char* method,
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
        "PUBLISH %s:%s:feed %s",
        this->type, this->id, message
    );

    free(message);

    return 0;
}


/*
 * Callbacks
 */

string stringify (attr_value v)
{
    string result = new string();
    switch (v.type) {
        case ATTR_STR:
            result = *v.value;
            break;
        case ATTR_INT:
            result = itoa(*((int*)(v.value)));
            break;
        case ATTR_ENTITY:
            result = ((Entity*)(v.value))->id);
            break;
        case ATTR_STREAM:
            result = ((Stream*)(v.value))->id);
            break;
    }
    return result;
}


void parse (attr_value *v, string s)
{
    switch (v->type) {
        case ATTR_STR:
            v->value = new string(s);
            break;
        case ATTR_INT:
            v->value = atoi(s.c_str());
            break;
    }
}


void _onCreated (redisAsyncContext *c,
                 void *r,
                 void *priv)
{
    Entity *s  = priv;

    if (s->onCreated != NULL) {
        s->onCreated(s);
    }
}


void _onMessage (redisAsyncContext *c,
                 void *r,
                 void *priv)
{
    Entity *s  = priv;
    redisReply *reply = r;
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
    void** pts = priv;
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
    Entity *s = priv;

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
