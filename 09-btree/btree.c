#include <solution.h>
#include <stdio.h>
#include <stdlib.h>

struct btree
{
    bool not_initialized;
    int value;
    struct btree *left;
    struct btree *right;
};

size_t node_size() {
    return sizeof(struct btree);
}

struct btree* minValueNode(struct btree* node)
{
    struct btree* current = node;
  
    while (current && current->left != NULL) {
        current = current->left;
    }
  
    return current; // самый глубокий левый node
}

struct btree* btree_alloc(unsigned int L)
{
    if (!L) {
        return NULL;
    }

    struct btree *head = (struct btree *)malloc(node_size());
    head->not_initialized = true; /* чтобы обработать случай, когда дерево пустое */
    head->value = 0;
    head->left = btree_alloc((L-1)/2);
    head->right = btree_alloc((L-1)/2);

    return head;
}

void btree_free(struct btree *t)
{
    // рекурсивно очищаем
    if (t == NULL) {
        return;
    }
    btree_free(t->left);
    btree_free(t->right);
    free(t);
}

void btree_insert(struct btree *t, int x)
{
    
    if (t->not_initialized == true) { /* fitst node case */
        t->value = x;
        t->not_initialized = false;
        return;
    }
    
    if (x > t->value) {
        if (t->right == NULL) {
            struct btree *right = (struct btree*)malloc(node_size());
            right->not_initialized = false;
            right->value = x;
            t->right = right;
        } else {
            btree_insert(t->right, x); /* рекурсивное добавление вправо */
        }
        return
    }
    if (x < t->value){
        if (t->right == NULL) {
            struct btree *left = (struct btree *)malloc(node_size());
            left->not_initialized = false;
            left->value = x;
            t->right = left;
        } else {
            btree_insert(t->left, x); /* рекурсивное добавление влево */
        }
    }
}

struct btree* deleteNode(struct btree* root, int x)
{
    if (root == NULL) {
        return root;
    }
  
    if (x < root->value) {
        root->left = deleteNode(root->left, x);
    }
  
    else if (x > root->value) {
        root->right = deleteNode(root->right, x);
    }
    else {
        if (root->left == NULL) {
            struct btree* temp = root->right;
            free(root);
            return temp;
        }
        else if (root->right == NULL) {
            struct btree* temp = root->left;
            free(root);
            return temp;
        }
        struct btree* temp = minValueNode(root->right);
  
        root->value = temp->value;
        
        root->right = deleteNode(root->right, temp->value);
    }
    return root;
}

void btree_delete(struct btree *t, int x)
{
    deleteNode(t, x);
}

bool btree_contains(struct btree *t, int x)
{
    if (t == NULL) {
        return false;
    } else {
        
        if (t->not_initialized) {
            return false;
        }
        
        int value = t->value;
        
        if (value == x) {
            return true;
        }
        if (x > value) {
            return btree_contains(t->right, x);
        }
        else {
            return btree_contains(t->left, x);
        }
    }
    return false;
}

struct btree_iter
{
    int *sorted_values;
    int count;
    int index;
};

void inOrder(struct btree *node, struct btree_iter *iter) {
    
    if (node == NULL) {
      return;
    }
    inOrder(node->left, iter);
    if (iter->count == 0) {
        iter->sorted_values = malloc(sizeof(int));
        (iter->sorted_values)[0] = node->value;
        iter->count = 1;
    }
    else {
        int *tmp = iter->sorted_values;
        int *new_sorted_values = malloc(sizeof(int) * (iter->count + 1));
        for (int i = 0; i < iter->count; i++) {
            (new_sorted_values)[i] = tmp[i];
        }
        free(tmp);
        new_sorted_values[iter->count] = node->value;
        int new_count = iter->count + 1;
        iter->count = new_count;
        iter->sorted_values = new_sorted_values;
    }
   
    inOrder(node->right, iter);
}

struct btree_iter* btree_iter_start(struct btree *t)
{
    struct btree_iter *iter = (struct btree_iter *)malloc(sizeof(struct btree_iter));
    iter->count = 0;
    
    inOrder(t, iter);
    
    iter->index = 0;
	return iter;
}

void btree_iter_end(struct btree_iter *i)
{
    free(i->sorted_values);
    free(i);
}

bool btree_iter_next(struct btree_iter *i, int *x)
{
    if (i->index == i->count) {
        return false;
    }
    
    int value = (i->sorted_values)[i->index];
    *x = value;
    i->index = i->index + 1;

    return true;
}
