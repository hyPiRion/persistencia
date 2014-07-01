/*
 * Copyright (c) 2014 Jean Niklas L'orange. All rights reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 *
 */

/*
 * This is the vanilla implementation of a persistent vector. This does not
 * include a tail, transient conversions, nor a display.
 */

#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "pvec.h"
#include "pvec_alloc.h"

// This is a trie node. It is always the branching factor size (unused table
// entries will be NULL).
typedef struct Node {
  struct Node *child[PVEC_BRANCHING];
} Node;

struct _Pvec {
  // The size of the vector.
  uint32_t size;
  // The height of the vector, represented as a shift.
  uint32_t shift;
  // The root of the vector.
  Node* root;
};

// The empty node.
static Node EMPTY_NODE = {.child = {0}};

// An empty vector.
static Pvec EMPTY_VECTOR = {.size = 0, .shift = 0, .root = &EMPTY_NODE};

// These are just prototypes -- no need to worry about these.
static inline Node *node_create(void);
static inline Node *node_clone(void);
static inline Pvec* pvec_clone(const Pvec *pvec);

// pvec_create just returns the empty vector.
const Pvec* pvec_create() {
  return &EMPTY_VECTOR;
}

// pvec_count just returns the size value inside the vector head.
uint32_t pvec_count(const Pvec *pvec) {
  return pvec->size;
}

void* pvec_nth(const Pvec *pvec, uint32_t index) {
  Node *node = pvec->root;
  for (uint32_t s = pvec->shift; s > 0; s -= PVEC_BITS) {
    int subindex = (index >> s) & PVEC_MASK;
    node = node->child[subindex];
  }
  // This last call is here because unsigned integers cannot be negative, thus
  // `s >= 0` will always be true.
  return (void *) node->child[index & PVEC_MASK];
}

const Pvec* pvec_update(const Pvec *restrict pvec, uint32_t index,
                        const void *restrict elt) {
  Pvec *clone = pvec_clone(pvec);
  Node *node = node_clone(pvec->root);
  clone->root = node;
  for (uint32_t s = pvec->shift; s > 0; s -= PVEC_BITS) {
    int subindex = (index >> s) & PVEC_MASK;
    node->child[subindex] = node_clone(node->child[subindex]);
    node = node->child[subindex];
  }
  int subindex = index & PVEC_MASK;
  node->child[subindex] = (Node *) elt;
  return (const Pvec*) clone;
}

// pvec_push is equivalent to the append function described in Section 2.5, but
// with bitwise access tricks.
const Pvec* pvec_push(const Pvec *restrict pvec, const void *restrict elt) {
  Pvec *clone = pvec_clone(pvec);
  uint32_t index = pvec_count(pvec);
  clone->size = pvec->size + 1;
  // this is the d_full(P) check for bit vectors
  if (pvec_count(pvec) == (M << pvec->shift)) {
    Node *new_root = node_create();
    new_root->child[0] = pvec->root;
    clone->root = new_root;
    clone->shift = pvec->shift + PVEC_BITS;
  }
  else {
    clone->root = node_clone(pvec->root);
  }
  Node *node = clone->root;
  for (uint32_t s = pvec->shift; s > 0; s -= PVEC_BITS) {
    int subindex = (index >> s) & PVEC_MASK;
    if (node->child[subindex] == NULL) { // the create part of clone-or-create
      node->child[subindex] = node_create();
    }
    else { // the clone part of clone-or-create
      node->child[subindex] = node_clone(node->child[subindex]);
    }
    node = node->child[subindex];
  }
  int subindex = index & PVEC_MASK;
  node->child[subindex] = (Node *) elt;
  return (const Pvec*) clone;
}

const Pvec* pvec_pop(const Pvec *pvec) {
  Pvec *clone = pvec_clone(pvec);
  uint32_t index = pvec_count(pvec) - 1;
  clone->size = pvec_count(pvec) - 1;
  // For bitwise tricks, we don't need to check that the new size != 1. M << 0
  // is M, so no problem.
  if (pvec_count(clone) == (M << pvec->shift)) {
    clone->shift = pvec->shift - PVEC_BITS;
    clone->root = pvec->root->child[0];
    return clone;
  }
  else {
    Node *node = node_clone(pvec->root);
    clone->root = node;
    for (uint32_t s = pvec->shift; s > 0; s -= PVEC_BITS) {
      int subindex = (index >> s) & PVEC_MASK;
      if (index & (((2*PVEC_BITS) << s) - 1) == 0) {
        node->child[subindex] = NULL;
        return clone;
      }
      else {
        node->child[subindex] = node_clone(node->child[subindex]);
        node = node->child[subindex];
      }
    }
    return clone;
  }
}
