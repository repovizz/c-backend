/*
 * STREAM class
 * Models a stream that sends frames of binary data to the clients
 * Usually put inside another entity that takes care of high-level interactions
 */

#ifndef __STREAM_H__
#define __STREAM_H__

#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <math.h>

#include <event.h>
#include <hiredis/hiredis.h>
#include <hiredis/async.h>

#include "../lib/json.h"

/*
 * Define some compile-time constants
 */

#ifndef CLIENT_ID
 #define CLIENT_ID "c_stream"     // Default feedback rejection ID
#endif

/*
 * This is the Stream dictionary
 * The attributes are predefined
 * Custom things should probably go in the source
 * Still have to decide how to add sampling rate, etc.
 */

typedef struct Stream_s {
    int frameLength;   //
    int frameRate;     // Should put these in an associative array
    int dimensions;    //
    unsigned int address; // JUST FOR THE DEMO
    char *id;
    redisAsyncContext *redisContext;
    void (*onCreated)(struct Stream_s *);
    void (*onUpdated)(struct Stream_s *);
    void (*onPolled)( struct Stream_s *);
    struct event timer;
    struct timeval interval;
    void *priv;
} Stream_t;

/*
 * Define the Stream callback type, just for convenience
 */

typedef void (*Stream_cb_f)(struct Stream_s *);

/*
 * Create a new Stream instance
 * Allocates and inits the dictionary, and sends everything to Redis
 * If the given pointer is already allocated, it takes defaults from there
 */

Stream_t* Stream_create (
    redisAsyncContext *c,       // Redis context to use (update & publish)
    redisAsyncContext *subs,    // Redis context to use (subscribe)
    char *id,                   // ID of the new stream
    Stream_cb_f callback        // Callback to call when Redis acknowleges
);

/*
 * Update one attribute of a Stream
 * Modifies the Stream and sends everything to Redis
 */

int Stream_update (
    Stream_t *s,                // Stream to update
    char *field,                // Name of the field to be updated
    int value                   // Value of the field
);

/*
 * Send a data frame to the clients
 * Adds the required headers and publishes it to Redis
 */

int Stream_sendFrame (
    Stream_t *s,                // Stream to update
    char *data,                 // Byte array containing the actual frame
    size_t length               // Length of the array
);

/*
 * Publish a message to everybody listening to the streams
 * This is used internally to notifiy other clients of every action
 */

int Stream_publishEvent (
    Stream_t *s,                // Stream to publish from
    char *method,               // Method of the event (update, delete...)
    char *data                  // JSON-encoded message
);

/*
 * Start the timer and call onPolled for updates
 * Avoid writing a timer if data is not being pushed to save dev time
 */

int Stream_startPolling (
    Stream_t *s                 // Stream to poll from
);

/*
 * Stop the timer
 * Analagous to startPolling
 */

int Stream_stopPolling (
    Stream_t *s                 // Stream to stop polling from
);

#endif