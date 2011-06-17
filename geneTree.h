// geneTree.h

#ifndef _GENE_TREE_H_
#define _GENE_TREE_H_

#include <stdlib.h>
#include <stdio.h>
#include "ALE.h"

#define START_LENGTH        5

struct LeafNode{
  int length;
  int current;
  int indicies[][2];
};

struct TreeBranch{
  struct TreeBranch *subBranches[4];// = {NULL,NULL,NULL,NULL};
  struct LeafNode *leaf;
};

typedef struct LeafNode leafNode_t;
typedef struct TreeBranch treeBranch_t;

int AddSeqToTree(const char sequence[], const int offset, const int klen, treeBranch_t *pRoot, const int assemPart);

int OutputIndicies(treeBranch_t *pRoot, const char sequence[], const int klen, int index[][2]){
  int i;
  treeBranch_t *currentBranch = pRoot;
  treeBranch_t *nextBranch;
  for(i = 0; i < klen; i++){
    if(sequence[i] == 'A'){
      if(currentBranch->subBranches[0] == NULL){
	return -1;
      }else{
	nextBranch = currentBranch->subBranches[0];
      }
    }
    if(sequence[i] == 'T'){
      if(currentBranch->subBranches[1] == NULL){
	return -1;
      }else{
	nextBranch = currentBranch->subBranches[1];
      }
    }
    if(sequence[i] == 'C'){
      if(currentBranch->subBranches[2] == NULL){
	return -1;
      }else{
	nextBranch = currentBranch->subBranches[2];
      }
    }
    if(sequence[i] == 'G'){
      if(currentBranch->subBranches[3] == NULL){
	return -1;
      }else{
	nextBranch = currentBranch->subBranches[3];
      }
    }
    currentBranch = nextBranch;
  }
  //printf("Number indicies: %i.\n", currentBranch->leaf->current);
  for(i = 0; i < currentBranch->leaf->current; i++){
    index[i][0] = currentBranch->leaf->indicies[i][0];
    index[i][1] = currentBranch->leaf->indicies[i][1];
  }
  return currentBranch->leaf->current;
}

treeBranch_t MakeTree(const assembly_t theAssem, const int klen, const int NUM_ASSEMBLY_PARTS){
  
  int i, j;
  treeBranch_t theRoot, *pRoot = &theRoot;
  theRoot.subBranches[0] = NULL;
  theRoot.subBranches[1] = NULL;
  theRoot.subBranches[2] = NULL;
  theRoot.subBranches[3] = NULL;
  theRoot.leaf = NULL;
  for(i = 0; i < NUM_ASSEMBLY_PARTS; i++){
    for(j = 0; j < theAssem.assemblyParts[i].seqLen - klen; j++){
      //printf("Adding sequence %i, %i to tree.\n", i, j);
      //printf("Attempting to add to tree: %i.\n", j);
      AddSeqToTree(theAssem.assemblyParts[i].sequence, j, klen, pRoot, i);
    }
  }
  return theRoot;
}

int AddSeqToTree(const char sequence[], const int offset, const int klen, treeBranch_t *pRoot, const int assemPart){
  //printf("Offset: %i, klen: %i, assemPart: %i\n", offset, klen, assemPart);
  int i;
  char currentRes;
  treeBranch_t *currentBranch = pRoot;
  treeBranch_t *nextBranch;
  leafNode_t *theLeaf, *oldLeaf;
  for(i = 0; i < klen; i++){
    //currentRes = getCharFromSeqByLoc(sequence, offset + i);
    currentRes = sequence[offset + i];
    //printf("Adding %c (res: %i).\n", currentRes, offset + i);
    if(currentRes == 'A'){
      if(currentBranch->subBranches[0] == NULL){
	//malloc another treebranch
	nextBranch = malloc(sizeof (treeBranch_t));
	nextBranch->subBranches[0] = NULL;
	nextBranch->subBranches[1] = NULL;
	nextBranch->subBranches[2] = NULL;
	nextBranch->subBranches[3] = NULL;
	nextBranch->leaf = NULL;
	currentBranch->subBranches[0] = nextBranch;
      }
      currentBranch = currentBranch->subBranches[0];
    }
    if(currentRes == 'T'){
      if(currentBranch->subBranches[1] == NULL){
	//malloc another treebranch
	nextBranch = malloc(sizeof (treeBranch_t));
	nextBranch->subBranches[0] = NULL;
	nextBranch->subBranches[1] = NULL;
	nextBranch->subBranches[2] = NULL;
	nextBranch->subBranches[3] = NULL;
	nextBranch->leaf = NULL;
	currentBranch->subBranches[1] = nextBranch;
      }
      currentBranch = currentBranch->subBranches[1];
    }
    if(currentRes == 'C'){
      if(currentBranch->subBranches[2] == NULL){
	//malloc another treebranch
	nextBranch = malloc(sizeof (treeBranch_t));
	nextBranch->subBranches[0] = NULL;
	nextBranch->subBranches[1] = NULL;
	nextBranch->subBranches[2] = NULL;
	nextBranch->subBranches[3] = NULL;
	nextBranch->leaf = NULL;
	currentBranch->subBranches[2] = nextBranch;
      }
      currentBranch = currentBranch->subBranches[2];
    }
    if(currentRes == 'G'){
      if(currentBranch->subBranches[3] == NULL){
	//malloc another treebranch
	nextBranch = malloc(sizeof (treeBranch_t));
	nextBranch->subBranches[0] = NULL;
	nextBranch->subBranches[1] = NULL;
	nextBranch->subBranches[2] = NULL;
	nextBranch->subBranches[3] = NULL;
	nextBranch->leaf = NULL;
	currentBranch->subBranches[3] = nextBranch;
      }
      currentBranch = currentBranch->subBranches[3];
    }
    if(currentRes == 'N'){
      return 0;
    }
  }

  //currentRes = getCharFromSeqByLoc(sequence, offset + klen - 1);
  //printf("Making a leaf.\n");
    if(currentBranch->leaf == NULL){
      //printf("malloc the leaf indicies.\n");
      theLeaf = malloc(2*sizeof(int) + 2*sizeof(int)*START_LENGTH);
      //printf("Initialize the leaf.\n");
      currentBranch->leaf = theLeaf;
      currentBranch->leaf->length = START_LENGTH;
      currentBranch->leaf->current = 0;
    }else{
      if(currentBranch->leaf->current > currentBranch->leaf->length - 1){
	// malloc a bigger leaf
	// printf("Making a bigger leaf.");
	oldLeaf = currentBranch->leaf;
	theLeaf = malloc(2*sizeof(int) + 2*sizeof(int)*(currentBranch->leaf->length + START_LENGTH));
	theLeaf->current = oldLeaf->current;
	theLeaf->length = currentBranch->leaf->length + START_LENGTH;
	for(i = 0; i < theLeaf->current; i++){
	  theLeaf->indicies[i][0] = oldLeaf->indicies[i][0];
	  theLeaf->indicies[i][1] = oldLeaf->indicies[i][1];
	}
	currentBranch->leaf = theLeaf;
	free(oldLeaf);
      }
    }
    //printf("Set indicies.\n");
    currentBranch->leaf->indicies[currentBranch->leaf->current][0] = assemPart;
    currentBranch->leaf->indicies[currentBranch->leaf->current][1] = offset;
    currentBranch->leaf->current += 1;
    return 1;
}

#endif
