#include "tools/list.h"
#include "tools/log.h"

void list_init(list_t *list){
    list->last = list->first = (list_node_t*)0;
    list->count = 0;

}


void list_insert_first(list_t *list,list_node_t *node){
    node->next = list->first;
    node->pre = (list_node_t*)0;

    if(list_is_empty(list))
    {
        list->first = node;
        list->last  = node;
    }
    else{
        list->first->pre = node;
        list->first = node;
    }
    list->count++;

}
void list_insert_last(list_t *list,list_node_t *node){
    node->pre = list->last;
    node->next = (list_node_t*)0;
    if(list_is_empty(list)){
        list->first = node;
        list->last  = node;
    }
    else{

        list->last->next = node;
        list->last = node;
    }
    list->count++;

}

// 删除第一个节点
list_node_t *list_remove_first(list_t *list){
    if(list_is_empty(list)){
        return (list_node_t*)0;
    }
    list_node_t *remove_node = list->first;
    list->first = remove_node->next;
    if(list->first==(list_node_t*)0){
        list->last = (list_node_t*)0;
    }
    else{
        list->first->pre = (list_node_t*)0;
    }
    remove_node->pre = remove_node->next = (list_node_t*)0;
    list->count--;
    return remove_node;
}

// 删除任意一个节点
list_node_t *list_node_remove(list_t* list,list_node_t *node){
    int flag = 0;
    if(node == list->first)
    {
        list->first = node->next;
        flag=1;
    }
    if(node == list->last)
    { 
        list->last = node->pre;
        flag=1;
    }

    if(node->pre)
    {
        node->pre->next = node->next;
        flag=1;
    }
    if(node->next)
    {
        node->next->pre = node->pre;
        flag=1;
    }
    if(flag)
    {
        node->pre = (list_node_t*)0;
        node->next = (list_node_t*)0;
        list->count--;
        return node;
    }
    else{
        log_printf("node not exist");
        return (list_node_t*)0;
    }

}