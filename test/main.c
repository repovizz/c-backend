#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <math.h>

#include <hiredis/hiredis.h>
#include <hiredis/async.h>
#include <hiredis/adapters/libevent.h>

#include "../src/stream.h"

#define max(a,b) (((a) > (b)) ? (a) : (b))
#define min(a,b) (((a) < (b)) ? (a) : (b))

/*
 * CALLBACKS
 */

void onConnect(const redisAsyncContext *c, int status)
{
    if (status != REDIS_OK) {
        printf("Error: %s\n", c->errstr);
        return;
    }
    printf("Connected...\n");
}


void onDisconnect(const redisAsyncContext *c, int status)
{
    if (status != REDIS_OK) {
        printf("Error: %s\n", c->errstr);
        return;
    }
    printf("Disconnected...\n");
}


void onCreated(Stream_t *s)
{
    Stream_update(s, "frameLength", 256);
    Stream_update(s, "frameRate", 10);
    Stream_startPolling(s);
}


void onTimeout(Stream_t *s)
{
    int i = 0;
    int len = s->frameLength;
    char *data = malloc(len);

    for (i = 0; i < len; i++) {
        int r = ((double)rand() * 32) / RAND_MAX;
        double sig = (sin((6.28 * i)/len) + 1) * 128 + r;
        sig = max(sig, 0);
        sig = min(sig, 255);
        data[i] = (char) floor(sig);
    }

    Stream_sendFrame(s, data, len);
}


/*
 * MAIN
 */

int main (int argc, char **argv)
{

    signal(SIGPIPE, SIG_IGN);
    struct event_base *base = event_init();

    // INIT REDIS

    redisAsyncContext *c = redisAsyncConnect("fpga-server.lbl.gov", 6379);
    if (c->err) {
        printf("Error: %s\n", c->errstr);
        return 1;
    }

    redisAsyncContext *subs = redisAsyncConnect("fpga-server.lbl.gov", 6379);
    if (c->err) {
        printf("Error: %s\n", c->errstr);
        return 1;
    }

    redisLibeventAttach(c,base);
    redisAsyncSetConnectCallback(c,onConnect);
    redisAsyncSetDisconnectCallback(c,onDisconnect);

    redisLibeventAttach(subs,base);
    redisAsyncSetConnectCallback(subs,onConnect);
    redisAsyncSetDisconnectCallback(subs,onDisconnect);

    // INIT SOURCE

    Stream_t *s = Stream_create(c, subs, "c_stream", onCreated);
    s->onPolled = onTimeout;

    // START

    event_base_dispatch(base);
    return 0;

}