#ifndef LIST_H
#define LIST_H
#include "comm/types.h"
typedef  struct _list_node_t{
    struct _list_node_t* pre;
    struct _list_node_t* next;
    // uint32_t data;    
}list_node_t;


// 初始化节点
static inline void list_node_init(list_node_t *node){
    node->next =  node->pre = (list_node_t*)0;
}
// 获取节点的上一个节点
static inline list_node_t * list_node_pre(list_node_t *node){
    return node->pre;
}

// 获取节点的下一个节点
static inline list_node_t * list_node_next(list_node_t *node){
    return node->next;
}


typedef struct _list_t{
    list_node_t *first;
    list_node_t *last;
    int count;
}list_t;


void list_init(list_t *list);

// 判断是否为空
static inline int list_is_empty(list_t *list){
    return list->count == 0;
}

// 查询数量
static inline int list_count(list_t* list){
    return list->count;
}

// 取出第一个节点

static inline list_node_t * list_first(list_t* list){
    return list->first;
}

static inline list_node_t * list_last(list_t* list){
    return list->last;
}


void list_insert_first(list_t *list,list_node_t *node);
void list_insert_last(list_t *list,list_node_t *node);

list_node_t *list_remove_first(list_t *list);
list_node_t *list_node_remove(list_t* list,list_node_t *node);






// 通过这个宏  无论我们在哪个位置插入节点，都可以直到节点所在结构的位置
#define offset_in_parent(parent_type,node_name) \
        ((uint32_t)&(((parent_type *)0)->node_name)) 

#define parent_addr(node, parent_type, node_name) \
    ((uint32_t)node - offset_in_parent(parent_type,node_name))


#define list_node_parent(node,parent_type,node_name)  \
    ((parent_type*)(node ? parent_addr(node,parent_type,node_name):0))

#endif