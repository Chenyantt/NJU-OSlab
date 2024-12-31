#include "philosopher.h"

// TODO: define some sem if you need

int chops[PHI_NUM];

void init() {
  // init some sem if you need
  // TODO();
  for(int i = 0; i< PHI_NUM; ++i){
    chops[i] = sem_open(1);
  }
}

void philosopher(int id) {
  // implement philosopher, remember to call `eat` and `think`
  while (1) {
    // TODO();
    if(id % 2){
      P(chops[id]);
      P(chops[(id+1) % 5]);
    }else{
      P(chops[(id+1) % 5]);
      P(chops[id]);
    }
    eat(id);
    V(chops[id]);
    V(chops[(id+1) % 5]);
    think(id);
  }
}
