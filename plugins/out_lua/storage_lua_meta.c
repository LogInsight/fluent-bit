//
// Created by jc on 5/1/2016.
//

#include <sys/fcntl.h>
#include <fluent-bit/flb_utils.h>
#include <stdbool.h>
#include <unistd.h>
#include "storage_lua_meta.h"

void lua_meta_init(struct meta_list *list) {
    char meta_file_name[1024];
    meta_file_name[0] = '\0';
    int name_len = 0;
    name_len = snprintf(meta_file_name, 1024, "%s", "./meta_log");
    meta_file_name[name_len] = '\0';
    list->meta_fd = open(meta_file_name, O_RDWR);
    if (list->meta_fd < 0)
    {
        flb_error("open file %s failed", meta_file_name);
        return;
    }
    mk_list_init(&list->entries);
}

void lua_meta_destory(struct meta_list *list) {
    struct mk_list *head;
    struct meta_node *tmp;
    int size = 0;
    while ((size = mk_list_size(&list->entries)) > 0) {
        mk_list_foreach(head, &list->entries) {
            tmp = mk_list_entry(head, struct meta_node, _head);
            mk_list_del(&tmp->_head);
            free(tmp);
            break;
        }
    }
    if (size != 0) {
        flb_error("list destory not complete, size = %d", size);
    }
    if (close(list->meta_fd) < 0) {
        flb_error("can't close the meta file");
        return;
    }
}

struct meta_node *lua_meta_get(struct meta_list *list, uint64_t key) {
    struct mk_list *head;
    struct meta_node *tmp;
    mk_list_foreach(head, &list->entries) {
        tmp = mk_list_entry(head, struct meta_node, _head);
        if (tmp->key == key) {
            flb_info("call mk_list_foreach %u", tmp->key);
            return tmp;
        }
    }
    tmp = (struct meta_node *) malloc(sizeof(struct meta_node));
    tmp->key = key;
    mk_list_add(&tmp->_head, &list->entries);
    return tmp;
}

void lua_meta_del(struct meta_list *list, uint64_t key) {
    struct mk_list *head;
    struct meta_node *tmp;
    mk_list_foreach(head, &list->entries) {
        tmp = mk_list_entry(head, struct meta_node, _head);
        if (tmp->key == key) {
            mk_list_del(&tmp->_head);
            free(tmp);
            return;
        }
    }
}

void lua_meta_debug_display(struct meta_list *list) {
    struct mk_list *head;
    struct meta_node *tmp;
    mk_list_foreach(head, &list->entries) {
        tmp = mk_list_entry(head, struct meta_node, _head);
        flb_info("display fd = %u", tmp->key);
    }
}
