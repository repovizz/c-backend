/*
 * ENTITY class
 * Models an entity that may send frames of binary data to the clients
 */

#ifndef __ENTITY_H__
#define __ENTITY_H__

#include <cstdlib>
#include <cstdio>
#include <cmath>
#include <string>
#include <map>

#include <event.h>
#include <hiredis/hiredis.h>
#include <hiredis/async.h>

#include "../lib/json.h"

#include "stream.h"

/*
 * Define some compile-time constants
 */

#ifndef CLIENT_ID
 #define CLIENT_ID "c_entity"     // Default feedback rejection ID
#endif

/*
 * This is the Entity dictionary
 * The attributes are predefined
 * Custom things should probably go in the source
 * Still have to decide how to add sampling rate, etc.
 */

typedef enum {
    ATTR_STRING,
    ATTR_INT,
    ATTR_ENTITY,
    ATTR_STREAM
} attr_type;

typedef struct {
    type: attr_type;
    value: void*;
} attr_value;

typedef std::map<std::string, attr_value> attr_map;
typedef std::pair<std::string, attr_value> attr;

class Entity {
public:
    attribute_map attrs;
    attribute_map changedAttrs;
    char *id;
    redisAsyncContext *redisContext;
    void (*onCreated)(Entity *);
    void (*onUpdated)(Entity *);
    void (*onPolled)(Entity *);
    struct event timer;
    struct timeval interval;
    void *priv;

    Entity(redisAsyncContext *c,
           redisAsyncContext *subs,
           char *type,
           char *id,
           void (*callback)(Entity *),
           attr_map *init_attrs);

    int update (char *field, attr_value *value);
    int publishEvent (char *method, char *data);
    int startPolling ();
    int stopPolling ();
};

/*
 * Define the Entity callback type, just for convenience
 */

typedef void (*Entity_cb_f)(Entity);

#endif
