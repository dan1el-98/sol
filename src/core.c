/* BSD 2-Clause License
 *
 * Copyright (c) 2019, Andrea Giacomo Baldan
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * * Redistributions of source code must retain the above copyright notice, this
 *   list of conditions and the following disclaimer.
 *
 * * Redistributions in binary form must reproduce the above copyright notice,
 *   this list of conditions and the following disclaimer in the documentation
 *   and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <string.h>
#include "util.h"
#include "core.h"

struct client *sol_client_new(struct connection *c) {
    struct client *client = xcalloc(1, sizeof(*client));
    client->conn = *c;
    client->online = true;
    client->clean_session = true;
    client->client_id[0] = '\0';
    client->last_action_time = time(NULL);
    client->lwt_msg = NULL;
    client->session = sol_session_new();
    return client;
}

void client_init(struct client *client) {
    client->online = true;
    client->clean_session = true;
    client->client_id[0] = '\0';
    client->last_action_time = time(NULL);
    client->lwt_msg = NULL;
    client->session = sol_session_new();
}

struct topic *topic_new(const char *name) {
    struct topic *t = xmalloc(sizeof(*t));
    topic_init(t, name);
    return t;
}

static int subscriber_destroy(struct hashtable_entry *entry) {

    if (!entry)
        return -HASHTABLE_ERR;

    // free value field
    if (entry->val) {
        struct subscriber *sub = entry->val;
        if (sub->refs == 1)
            xfree(entry->val);
        else
            sub->refs--;
    }

    return HASHTABLE_OK;
}

void topic_init(struct topic *t, const char *name) {
    t->name = name;
    t->subscribers = hashtable_new(subscriber_destroy);
    t->retained_msg = NULL;
}

void topic_add_subscriber(struct topic *t,
                          struct client *client,
                          unsigned qos,
                          bool cleansession) {
    struct subscriber *sub = xmalloc(sizeof(*sub));
    sub->client = client;
    sub->qos = qos;
    sub->refs = 1;
    hashtable_put(t->subscribers, sub->client->client_id, sub);

    // It must be added to the session if cleansession is false
    if (!cleansession)
        client->session->subscriptions =
            list_push(client->session->subscriptions, t);

}

void topic_del_subscriber(struct topic *t,
                          struct client *client,
                          bool cleansession) {
    cleansession = (bool) cleansession; // temporary placeholder for compiler
    hashtable_del(t->subscribers, client->client_id);

    // TODO remove in case of cleansession == false
}

void sol_topic_put(struct sol *sol, struct topic *t) {
    trie_insert(&sol->topics, t->name, t);
}

void sol_topic_del(struct sol *sol, const char *name) {
    trie_delete(&sol->topics, name);
}

bool sol_topic_exists(struct sol *sol, const char *name) {
    struct topic *t = sol_topic_get(sol, name);
    return t != NULL;
}

struct topic *sol_topic_get(struct sol *sol, const char *name) {
    struct topic *ret_topic;
    trie_find(&sol->topics, name, (void *) &ret_topic);
    return ret_topic;
}

struct topic *sol_topic_get_or_create(struct sol *sol, const char *name) {
    struct topic *t = sol_topic_get(sol, name);
    if (!t) {
        t = topic_new(xstrdup(name));
        sol_topic_put(sol, t);
    }
    return t;
}

struct inflight_msg *inflight_msg_new(struct client *c,
                                      struct mqtt_packet *p,
                                      int type, size_t size) {
    struct inflight_msg *imsg = xmalloc(sizeof(*imsg));
    imsg->client = c;
    imsg->sent_timestamp = time(NULL);
    imsg->packet = p;
    imsg->type = type;
    imsg->size = size;
    return imsg;
}

struct session *sol_session_new(void) {
    struct session *s = xmalloc(sizeof(*s));
    // TODO add a subscription destroyer
    s->subscriptions = list_new(NULL);
    s->msg_queue = xmalloc(sizeof(struct inflight_msg) * 4);
    s->msg_queue_next = 0;
    s->msg_queue_size = 0;
    return s;
}

void sol_session_append_imsg(struct session *s, struct inflight_msg *m) {
    if (s->msg_queue_next > s->msg_queue_size / 2) {
        s->msg_queue_size *= 2;
        s->msg_queue = xrealloc(s->msg_queue, s->msg_queue_size);
    }
    s->msg_queue[s->msg_queue_next++] = m;
}

unsigned next_free_mid(struct inflight_msg **i_msgs) {
    unsigned short mid = 0;
    for (unsigned int i = 1; i < MAX_INFLIGHT_MSGS; ++i) {
        if (i_msgs[i] == NULL) {
            mid = i;
            break;
        }
    }
    return mid;
}
