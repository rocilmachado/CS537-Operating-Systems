//Include files
#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<pthread.h>
#include<sys/stat.h>
#include<semaphore.h>
#include"mapreduce.h"

typedef struct __vNode {
        char* val;
        struct __vNode *next;
} vNode;

// Store key value pair from mapper
typedef struct __kvNode {
        char *key;
        struct __vNode *valHead;
        struct __kvNode *nextKey;
} kvNode;

typedef struct __partition {
        pthread_t threadID;
        struct __kvNode *listHead;
        pthread_mutex_t lock;
} partition;

int numMappers;
int numReducers;
struct __partition **mapperStructure;
struct __partition **reducerStructure;
vNode **mapHead;
vNode **redHead;
int imap = 0;
int ired = 0;
int ipart = 0;
int numPartitions;

pthread_mutex_t imapLock;
pthread_mutex_t iredLock;
pthread_mutex_t ipartLock;


char **fileNames;
int numFiles;
int numFilesLeft;
pthread_mutex_t fileLock;

// Function declarations
Mapper mapperFunc;
Combiner combinerFunc;
Reducer reducerFunc;
Partitioner partitionFunc;

// Step 2 : Implementation APIs

unsigned long MR_DefaultHashPartition(char *key, int num_partitions) {
    unsigned long hash = 5381;
    int c;
    while ((c = *key++) != '\0')
        hash = hash * 33 + c;
    return hash % num_partitions;
}

void MR_EmitToCombiner(char *key, char *value) {
        pthread_t tid = pthread_self();
        int id = -1 ;
        for(int i=0; i < numMappers; i++) {
               if(mapperStructure[i] != NULL && mapperStructure[i]->threadID == tid) {
                       id = i;        
                       break;
               }
        }

        kvNode *keyFound  = NULL;

        if(mapperStructure[id]->listHead != NULL) {
        kvNode *temp  = NULL;
        temp  = mapperStructure[id]->listHead;
        while(temp!=NULL){
               if(strcmp(temp->key,key) == 0){
                       keyFound = temp;
                       break;
               }
               temp = temp->nextKey;
        }
        temp= NULL; 
        }

        if(keyFound == NULL){
               kvNode *newKeyList = (kvNode *)malloc(sizeof(kvNode));
                newKeyList->key = strdup(key);
                newKeyList->valHead = NULL;
               vNode *newVal = (vNode*)malloc(sizeof(vNode));
               newVal->val = strdup(value);
                newVal->next = newKeyList->valHead;
               newKeyList->valHead = newVal;
               newKeyList->nextKey = mapperStructure[id]->listHead;
                mapperStructure[id]->listHead = newKeyList;
        }       else {
               vNode *newVal = (vNode*)malloc(sizeof(vNode));
               newVal->val = strdup(value);
               newVal->next = keyFound->valHead;
               keyFound->valHead = newVal;
        }
        
        keyFound = NULL;

}


void MR_EmitToReducer(char *key, char *value) {
        unsigned long hashNum = partitionFunc(key, numPartitions);       
        
        kvNode *keyFound  = NULL;
               pthread_mutex_lock(&reducerStructure[hashNum]->lock);
        kvNode *temp  = reducerStructure[hashNum]->listHead;
        while(temp != NULL){
                if(strcmp(temp->key,key) == 0){
                        keyFound = temp;
                        break;
                }
                temp = temp->nextKey; 
        }
        if(keyFound == NULL){
                kvNode *newKeyList = (kvNode *)malloc(sizeof(kvNode));
                newKeyList->key = strdup(key);
                newKeyList->valHead = NULL;
                vNode *newVal = (vNode*)malloc(sizeof(vNode));
                newVal->val = strdup(value);
                newVal->next = newKeyList->valHead;
                newKeyList->valHead = newVal;
                newKeyList->nextKey = reducerStructure[hashNum]->listHead;
                reducerStructure[hashNum]->listHead = newKeyList;
        }
        else {
                vNode *newVal = (vNode*)malloc(sizeof(vNode));
                newVal->val = strdup(value);
               newVal->next = keyFound->valHead;
                keyFound->valHead = newVal;
        }
        pthread_mutex_unlock(&reducerStructure[hashNum]->lock);
}


char *getNextCombiner(char* key) {
        pthread_t tid = pthread_self();
        int id = -1 ;
        vNode *temp = (vNode*)malloc(sizeof(vNode));
        char *returnVal = NULL;
        for(int i=0; i < numMappers; i++) {
                if(mapperStructure[i] != NULL && mapperStructure[i]->threadID == tid) {
                        id = i;
                        break;
                }
        }
        kvNode *curr = mapperStructure[id]->listHead;
        vNode *val = NULL;
        while(curr != NULL){
               if(strcmp(curr->key,key) == 0 && curr->valHead != NULL) {
                       returnVal = strdup(curr->valHead->val);
                       temp->val = returnVal;
                       temp->next = mapHead[id];
                       mapHead[id] = temp;
                       val = curr->valHead;
                       curr->valHead = val->next;
                       free((char*) val->val);
                       free(val);
                       return returnVal;
               }
                curr = curr->nextKey;
        }

        vNode *valList = mapHead[id];
        while(mapHead[id] != NULL){
               valList = mapHead[id];
               mapHead[id] = mapHead[id]->next;    
               free((char *) valList->val);
               free(valList);
        }
        free(temp);
        return NULL;
}


char* getNextReducer(char* key, int partition_number) {
        pthread_t tid = pthread_self();
        kvNode *curr = reducerStructure[partition_number]->listHead;
        vNode *val = NULL;
        int id = -1;
        char *returnVal = NULL;
        vNode *temp = (vNode*)malloc(sizeof(vNode));
        
        for(int i=0; i < numReducers; i++) {
                if(reducerStructure[i] != NULL && reducerStructure[i]->threadID == tid) {
                        id = i;
                        break;
                }
        }
        
        
        while(curr != NULL){
                if(strcmp(curr->key, key) == 0 && curr->valHead != NULL) {
                      returnVal = strdup(curr->valHead->val);
                       temp->val = returnVal;
                        temp->next = redHead[id];
                        redHead[id] = temp;
                       val = curr->valHead;
                        curr->valHead = val->next;
                       free((char*) val->val);
                        free(val);
                        return returnVal;
                }
               curr = curr->nextKey;
        }
        vNode *valList = redHead[id];
        while(redHead[id] != NULL){
                valList = redHead[id];
                redHead[id] = redHead[id]->next;
                free((char *) valList->val);
                free(valList);
        }
        free(temp);

        return NULL;
}

void* mapperWrapper() {
        pthread_t tid = pthread_self();
        pthread_mutex_lock(&imapLock);
        int map_num = imap;
        imap++;
        pthread_mutex_unlock(&imapLock);
        
        mapperStructure[map_num] = malloc(sizeof(partition));
        if(mapperStructure[map_num] == NULL)
               exit(1);
        
        mapperStructure[map_num]->threadID = tid;
        mapperStructure[map_num]->listHead = NULL;
        

process:
        pthread_mutex_lock(&fileLock);
        char *file = NULL;
        
        if(numFilesLeft == 0) {
               pthread_mutex_unlock(&fileLock);
               pthread_exit(0);
        }
        
        for(int i = 0; i < numFiles; i++) {
               if(fileNames[i] == NULL)
                       continue;
               else {
                       file = strdup(fileNames[i]);
                       fileNames[i] = NULL;
                       free(fileNames[i]);
                       numFilesLeft--;
                       break;
               }       
        }
        pthread_mutex_unlock(&fileLock);
        if(file != NULL) {
               mapperFunc(file);
               free((char *) file);
        }

        char* value;
        if(numFilesLeft == 0){
               kvNode *curr = mapperStructure[map_num]->listHead;
               while(curr != NULL){
                       if(combinerFunc != NULL)
                               combinerFunc(curr->key, getNextCombiner);
                       else {
                               while((value = getNextCombiner(curr->key)) != NULL)
                                      MR_EmitToReducer(curr->key, value);
                       }

                       
                               
                       curr = curr->nextKey;
               
               }
               pthread_exit(0);
        }
        else
               goto process;


}

void* reducerWrapper() {
        pthread_mutex_lock(&iredLock);
        int partitionNum = ired;
        ired++;
        pthread_mutex_unlock(&iredLock);
        reducerStructure[partitionNum]->threadID = pthread_self();
        kvNode *temp = reducerStructure[partitionNum]->listHead;
        while(temp != NULL) {
               reducerFunc(temp->key, NULL, getNextReducer, partitionNum);
               temp = temp->nextKey;
        }
        pthread_exit(0);
}

void * initializeReducer() {
        pthread_mutex_lock(&ipartLock);
        int partitionNum = ipart;
        ipart++;
        pthread_mutex_unlock(&ipartLock);
        reducerStructure[partitionNum] = malloc(sizeof(partition));
        if(reducerStructure[partitionNum] == NULL)
               exit(1);
        pthread_mutex_init(&reducerStructure[partitionNum]->lock, NULL);
        reducerStructure[partitionNum]->listHead = NULL;
        pthread_exit(0);

}


void MR_Run(int argc, char *argv[], Mapper map, int num_mappers, Reducer reduce, int num_reducers, Combiner combine, Partitioner partition)
{       
      
        pthread_t mapperThreads[num_mappers];
        pthread_t reducerThreads[num_reducers];
        pthread_t reducerInit[num_reducers];

        imap = 0;
        ired = 0;
        ipart = 0;

        mapperFunc = map;
        combinerFunc = combine;
        reducerFunc = reduce;
        partitionFunc = partition;

        numMappers = num_mappers;
        numReducers = num_reducers;
        numPartitions = num_reducers;

        mapperStructure = NULL;      
        reducerStructure = NULL;      

        numFiles = argc - 1;
        numFilesLeft = numFiles;
        
        if(argc==1)
        {
                exit(1);
        }
        

        reducerStructure = malloc(num_reducers*sizeof(partition));
        for(int i = 0; i< num_reducers; i++) {
               reducerStructure[i] = NULL;
               pthread_create(&reducerInit[i], NULL, initializeReducer, NULL);
        
        }


        
        fileNames = malloc(numFiles*sizeof(char*));
        for(int i = 0; i < numFiles; i++) {
               fileNames[i] = argv[i+1];
        }

        for(int i=0; i<num_reducers; i++) {
                pthread_join(reducerInit[i], NULL);
        }

        
        mapperStructure = malloc(num_mappers*sizeof(partition));
        mapHead = malloc(num_mappers*sizeof(char*)); 
        
        for(int i=0; i<num_mappers; i++) {
               mapperStructure[i] = NULL;
               pthread_create(&mapperThreads[i], NULL, mapperWrapper, NULL);
               mapHead[i] = NULL;
        }
       
        for(int i=0; i<num_mappers; i++) {
                pthread_join(mapperThreads[i], NULL);
        }
        
        
        kvNode *temp = NULL;
        for(int i = 0; i<imap; i++) {
               kvNode *curr = mapperStructure[i]->listHead;
               while(curr != NULL){
               
                       free((char*) curr->key);
                       curr->key = NULL;
                       vNode *val;
                       while(curr->valHead != NULL){
                               val = curr->valHead;
                               curr->valHead = curr->valHead->next;
                               free((char*) val->val);
                               val->val = NULL;
                               free(val);
                               val = NULL;
                       }
                       free((vNode*) curr->valHead);
                       curr->valHead = NULL;
                       temp = curr->nextKey;
                       free(curr);
                       curr = NULL;
                       curr = temp;
        }
               temp = NULL;
        }
        
        for(int i = 0; i < num_mappers; i++) { 
               free(mapperStructure[i]);
               mapperStructure[i] = NULL;
        }
        free(mapperStructure);
        mapperStructure=NULL;
        free(fileNames);


        
        redHead = malloc(num_reducers*sizeof(char*));        
        for(int i=0; i < num_reducers;i++) {
               pthread_create(&reducerThreads[i], NULL, reducerWrapper, NULL);
               redHead[i] = NULL;
        }

        
        for(int i=0; i<num_reducers; i++) {
                pthread_join(reducerThreads[i], NULL);
        }

        temp = NULL;
        for(int i = 0; i < num_reducers; i++) {
                kvNode *curr = reducerStructure[i]->listHead;
                while(curr != NULL){
                        free((char*) curr->key);
                        vNode *val;
                        while(curr->valHead != NULL){
                                val = curr->valHead;
                                curr->valHead = curr->valHead->next;
                                free((char*) val->val);
                                free(val);

                        }
                        free((vNode*) curr->valHead);
                        temp = curr->nextKey;
                        free(curr);
                        curr = temp;
        }
        }


        for(int i = 0; i < num_reducers; i++) {
               free(reducerStructure[i]);
               reducerStructure[i] = NULL;
        }
        free(reducerStructure);
        free(mapHead); 
        free(redHead);
        mapHead=NULL;
        redHead=NULL;
}