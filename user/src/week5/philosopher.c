#include "philosopher.h"

// TODO: define some sem if you need
// usem_t forks[9];
int forks[PHI_NUM];

void init()
{
  // init some sem if you need
  for (uint32_t i = 0; i < PHI_NUM; ++i)
  {
    // sys_open相当于声明一个信号量
    forks[i] = sem_open(1);
  }

  // for (uint32_t i = 0; i < 9; ++i){
  //   printf("%d\n",forks[i]);
  // }
}

void philosopher(int id)
{

  // implement philosopher, remember to call `eat` and `think`
  while (1)
  {
    // printf("hello\n");
    if (id % 2 == 0)
    {
      sem_p(forks[id ]);
      sem_p(forks[(id + 1)%PHI_NUM]);
      eat(id);
      sem_v(forks[(id + 1)%PHI_NUM]);
      sem_v(forks[id]);
      think(id);
    }
    else
    {
      sem_p(forks[(id + 1)%PHI_NUM]);
      sem_p(forks[id]);
      eat(id);
      sem_v(forks[id]);
      sem_v(forks[(id + 1)%PHI_NUM]);
      think(id);
    }
  }
}
