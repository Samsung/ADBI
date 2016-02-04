#ifndef TREE_H_
#define TREE_H_

#include "likely.h"

#include <assert.h>
#include <stdbool.h>
#include <stdlib.h>

/*************************************************************************************************************
 * Structures and types
 ************************************************************************************************************/
typedef unsigned long tree_key_t;

typedef struct node_t {
    tree_key_t key;
    void * val;
    struct {
        struct node_t * left, * right, * parent;
    } links;
    int height;
} node_t;

typedef node_t * tree_t;

/* Forward declarations. */
static inline node_t * tree_get_node(const tree_t * tree, tree_key_t key);
static inline node_t * tree_iter_prev(const node_t * node);


/*************************************************************************************************************
 * Node insertion
 ************************************************************************************************************/
node_t * tree_try_insert_node(tree_t * tree, tree_key_t key);

static inline bool tree_try_insert(tree_t * tree, tree_key_t key, void * value) {
    node_t * node = tree_try_insert_node(tree, key);
    if (node) {
        node->val = value;
        return true;
    } else {
        return false;
    }
}

static inline node_t * tree_insert_node(tree_t * tree, tree_key_t key) {
    node_t * result = tree_try_insert_node(tree, key);
    assert(result);
    return result;
}

static inline void tree_insert(tree_t * tree, tree_key_t key, void * value) {
    tree_insert_node(tree, key)->val = value;
}

static inline void tree_update(tree_t * tree, tree_key_t key, void * value) {
    tree_get_node(tree, key)->val = value;
}

/*************************************************************************************************************
 * Node removal
 ************************************************************************************************************/
void tree_remove_node(tree_t * tree, node_t * node);

static inline bool tree_discard(tree_t * tree, tree_key_t key) {
    node_t * node = tree_get_node(tree, key);
    
    if (likely(node)) {
        tree_remove_node(tree, node);
        return true;
    } else {
        return false;
    }
}

static inline void tree_remove(tree_t * tree, tree_key_t key) {
    int ret = tree_discard(tree, key);
    assert(ret);
}

static inline void * tree_pop(tree_t * tree) {
    node_t * root = *tree;
    if (likely(root)) {
        void * ret = root->val;
        assert(ret);    /* values must not be NULL in this case */
        tree_remove_node(tree, root);
        return ret;
    } else {
        return NULL;
    }
}

/*************************************************************************************************************
 * Search functions
 ************************************************************************************************************/

static inline node_t * tree_get_node(const tree_t * tree, tree_key_t key) {
    node_t * node = *tree;
    
    while (node && (key != node->key)) {
        if (key < node->key) {
            node = node->links.left;
        } else {
            node = node->links.right;
        }
    }
    return node;
}

/* Return a node with key close to the given key. */
static inline node_t * tree_get_close_node(const tree_t * tree, tree_key_t key) {
    node_t * node = *tree;
    
    if (!node)
        return NULL;
        
    while (key != node->key) {
        if (key < node->key) {
            if (node->links.left)
                node = node->links.left;
            else
                return node;
        } else {
            if (node->links.right)
                node = node->links.right;
            else
                return node;
        }
    }
    return node;
}

static inline node_t * tree_get_node_le(const tree_t * tree, tree_key_t key) {

    node_t * close = tree_get_close_node(tree, key);
    
    if (!close) {
        /* tree is empty */
        return NULL;
    }
    
    if (close->key <= key) {
        return close;
    } else {
        return tree_iter_prev(close);
    }
}

static inline node_t * tree_get_node_range(const tree_t * tree, tree_key_t a, tree_key_t b) {
    node_t * node = *tree;
    
    while (node && !((a <= node->key) && (node->key < b))) {
        if (node->key < a) {
            node = node->links.left;
        } else {
            node = node->links.right;
        }
    }
    
    return node;
}

/************************************************************************************************************/

static inline void * tree_get(const tree_t * tree, tree_key_t key) {
    node_t * node = tree_get_node(tree, key);
    return node ? node->val : NULL;
}

static inline void * tree_get_le(const tree_t * tree, tree_key_t key) {
    node_t * node = tree_get_node_le(tree, key);
    return node ? node->val : NULL;
}

static inline void * tree_get_range(const tree_t * tree, tree_key_t a, tree_key_t b) {
    node_t * node = tree_get_node_range(tree, a, b);
    return node ? node->val : NULL;
}

/*************************************************************************************************************
 * Other functions
 ************************************************************************************************************/

void tree_clear(tree_t * tree);

void tree_update(tree_t * tree, tree_key_t key, void * value);

static inline int tree_contains(const tree_t * tree, tree_key_t key) {
    return tree_get_node((tree_t *) tree, key) != NULL;
}

/* Return non-zero if the tree is empty (has no nodes). */
static inline int tree_empty(tree_t * tree) {
    return (*tree) == NULL;
}

/*************************************************************************************************************
 * Node iteration
 ************************************************************************************************************/

/* Return the node with the lowest key starting at the given node. */
static inline node_t * tree_get_min(node_t * node) {
    if (likely(node))
        while (likely(node->links.left))
            node = node->links.left;
    return node;
}

/* Return the node with the lowest key starting at the given node. */
static inline node_t * tree_get_max(node_t * node) {
    if (likely(node))
        while (likely(node->links.right))
            node = node->links.right;
    return node;
}

/* Return the node with the lowest key in the tree. */
static inline node_t * tree_iter_start(const tree_t * tree) {
    return tree_get_min(*tree);
}

/* Return the value of any node in the tree. */
static inline void * tree_get_any_val(const tree_t * tree) {
    return *tree ? (*tree)->val : NULL;
}

/* Return the node with the lowest key, but greater than the key of the given
 * node. Return NULL if there is no such node or if the argument is NULL. */
static inline node_t * tree_iter_next(const node_t * node) {
    if (unlikely(!node))
        return NULL;
        
    if (node->links.right)
        return tree_get_min(node->links.right);
        
    while ((node->links.parent) && (node->links.parent->links.right == node))
        node = node->links.parent;
        
    return node->links.parent;
}

/* Return the node with the highest key, but lower than the key of the given
 * node. Return NULL if there is no such node or if the argument is NULL. */
static inline node_t * tree_iter_prev(const node_t * node) {
    if (unlikely(!node))
        return NULL;
        
    if (node->links.left)
        return tree_get_max(node->links.right);
        
    while ((node->links.parent) && (node->links.parent->links.left == node))
        node = node->links.parent;
        
    return node->links.parent;
}

#define TREE_ITER(tree, node)                                                                   \
    for (node_t * node = tree_iter_start(tree); node; node = tree_iter_next(node))

#define TREE_ITER_SAFE(tree, node)                                                              \
    for (node_t * node = tree_iter_start(tree), * node ## _next = tree_iter_next(node);         \
            node;                                                                               \
            node = node ## _next, node ## _next = tree_iter_next(node))

void * tree_pop(tree_t * tree);

/* Return the size of the tree (node count). */
static inline unsigned int tree_size(tree_t * tree) {
    unsigned int ret = 0;
    TREE_ITER(tree, node) {
        ++ret;
    }
    return ret;
}

#endif
