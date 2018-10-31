#include <stdio.h>
#include <urcu-qsbr.h>
#include <urcu/rculist.h>
#include <urcu/compiler.h>
#include <pthread.h>
#include <stdlib.h>
#include <unistd.h>



#define NR_READERS 100
#define NR_WRITERS 1


void *reader_fn(void *i);
void *writer_fn(void *i);
void free_node_rcu(struct rcu_head *head);


struct mynode {
  int value;      /* Node content */
  struct cds_list_head node;  /* Linked-list chaining */
  struct rcu_head rcu_head; /* For call_rcu() */
};


CDS_LIST_HEAD(mylist);

int main(){
  pthread_t readers[NR_READERS],writers[NR_WRITERS];
  
  int values[] = { -5, 42, 36, 24, };
  unsigned int i;
  int ret = 0;
  struct mynode *node, *n;

  for (i = 0; i < CAA_ARRAY_SIZE(values); i++) {
    node = malloc(sizeof(*node));
    if (!node) {
      ret = -1;
      goto end;
    }
    node->value = values[i];
    cds_list_add_tail_rcu(&node->node, &mylist);
  }

  /* Only one reader created for now which stalls all
     the threads indefinitely */

  for(int i = 0;i < NR_READERS;i++){
    int x = 1;
    pthread_create(&readers[i],NULL,reader_fn,(void *)x);
  }
  
  /* Writer threads keeps on updating the same item
    over and over */
  for(int i = 0; i<NR_WRITERS; i++){
    int x = 1;
    pthread_create(&writers[i],NULL,writer_fn,(void *)x);
  }


  //for(int i = 0;i<NR_READERS;i++)
    //pthread_join(readers[i], NULL);
  for(int i = 0;i<NR_WRITERS;i++)
    pthread_join(writers[i],NULL);
end:
  return 0;
}


void *reader_fn(void *x){
  rcu_register_thread();
  long cnt= 0;
  // Infinite Read Lock
  rcu_read_lock();
    while(1){
      cnt++;
      if(cnt%100000000 == 0){
        printf("reader alive");
        cnt = 0;
      }
    }
  rcu_read_unlock();
}

void *writer_fn(void *x){
  struct mynode *node,*n;
  int ret =0 ;
  while(true){
    cds_list_for_each_entry_safe(node, n, &mylist, node) {
      struct mynode *new_node;

      new_node = malloc(sizeof(*node));
      if (!new_node) {
        ret = -1;
        goto end;
      }
      /* Replacement node value is negated original value. */
      new_node->value = -node->value;
      cds_list_replace_rcu(&node->node, &new_node->node);
      call_rcu(&node->rcu_head, free_node_rcu);
    }
    continue;
    end:
      break;
  }
}

void free_node_rcu(struct rcu_head *head){
  struct mynode *node = caa_container_of(head, struct mynode, rcu_head);

  free(node);
}
