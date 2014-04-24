#include <stdio.h>
#include <stdlib.h>
#include <omp.h>
#include <math.h>

//function definitions
void get_input();  /* Read input from file */
void one_by_one();

//globals
int **a;
int num;
int num_paths;
int *shortest_path;
int shortest_dist;
int shortest_thread;

//globals for one_by_one()
int *cities_to_permute;
int num_paths_per_thread;
int thread_id_city;



void main(int argc, char *argv[]){
  get_input(argv[1]); //get matrix input
  int p = num-1;
  num_paths = 1;
  while (p>0) { //calculate (num-1)!
    num_paths *= p;
    p--;
  }
  num_paths_per_thread = num_paths / (num-1);

  //printf("Paths per thread: %d of %d\n",num_paths_per_thread,num_paths);

  shortest_path = (int *) malloc(num * sizeof(int));
  shortest_dist = 999999999; //unset, alg will check on first iteration and store anything

  one_by_one();
}


//--------------------------------------------------------------------------------------------------
/* ONE_BY_ONE algorithm
 * This technique, although it sounds sequential, flatly isn't. It is called "one_by_one"
 * because it only generates one permutation at a time. This saves a huge amount of memory.
 * 1) Test the current permutation.
 * 2) If it is smaller than stored permutation, we have found a better path.
 * 3) Generate the lexicographically next permutation, and loop back to step 1.
 *      If we have hit the end of the computational loop for this thread, wait for other threads.
 *      Then print the smallest found.
 */
void one_by_one() { //saves a massive amount of memory.
  //each thread starts with cities: 0->thread_id, and then permutes the rest
  //ONE AT A TIME.
  //after doing "get_next_permutation(0->thread_id)", each thread will check
  //how long that permutation is. if it's shorter, store it, if not, chuck it.

  //all threads will hold "City 0" as starting city, and will permute based on 
  //"0->rank->(permutations)"

  /* each thread calculates its own cities to permute
   * thread#  cities to withold
   * 0        0,1
   * 1        0,2
   * 2        0,3
   * n        0,n
   */
  #pragma omp parallel num_threads(num-1) private(cities_to_permute,thread_id_city)
  {
    cities_to_permute = (int *) malloc ((num-2) * sizeof(int));
    int i;

    /* Compute cities to permute in each thread */
    int index = 0;
    thread_id_city = omp_get_thread_num()+1;
    for (i=1;i<num;i++) {
      if (i != thread_id_city) cities_to_permute[index++] = i;
    }
    /* END compute cities to permute */

    /* Compute base distance, from 0->thread_id+1 */
    int base_distance = a[0][thread_id_city];
    int this_distance;
    /* Computational Loop
     * 1) Get next lexicographical permutation
     * 2) Test if it is smaller than current smallest
     *    -> If shortest_dist == -1, just use any of them.
     *    -> If distance of this path becomes longer thatn shortest_dist at any
     *       time, just drop it immediately and move on.
     */
    for (i=0;i<num_paths_per_thread;i++) {
    //while (1) {
      /* STEP 1: TEST THIS PERMUTATION
       *         Start with this_distance = base_distance (0->thread_id+1)
       */
      this_distance = base_distance;
      this_distance += a[thread_id_city][cities_to_permute[0]];
      for (i=1;i<num-2;i++) {  //loop to add distance in permutation
        this_distance += a[cities_to_permute[i-1]][cities_to_permute[i]];
        if (this_distance >= shortest_dist) break;  //if it's bigger, drop out of the loop.
      }

      /* STEP 2: CONDITIONAL WRITE
       *         If this_distance < previous shortest_distance, store it.
       */
      if (this_distance < shortest_dist) //we have a hit!
      {
        //now we have a critical section where it writes the new path to the path shortest_path.
        #pragma omp critical
        {
          if (this_distance < shortest_dist) //have to test TWICE! otherwise, we create an artifical race condition
          {
            shortest_dist = this_distance;
            shortest_path[0] = 0;
            shortest_path[1] = thread_id_city;
            for (i=0;i<num-2;i++)
              shortest_path[i+2] = cities_to_permute[i];
            shortest_thread = omp_get_thread_num();
          }//end if
        }//end #pragma omp critical
      }//end if

      /* STEP 3: GET NEXT NEW PERMUTATION LEXICOGRAPHICALLY 
       *         If this is the last permutation, just end this thread.
       */
      int x,y,temp;
      x = num-3;
      while (x>0 && cities_to_permute[x-1] >= cities_to_permute[x])
        x--;

      if (x==0) //last permutation, so break out of computational loop
        break;

      y = num-3;
      while(cities_to_permute[y] <= cities_to_permute[x-1])
        y--;

      temp = cities_to_permute[x-1];
      cities_to_permute[x-1] = cities_to_permute[y];
      cities_to_permute[y] = temp;

      y = num-3;
      while (x<y) {
        temp = cities_to_permute[x];
        cities_to_permute[x] = cities_to_permute[y];
        cities_to_permute[y] = temp;
        x++;
        y--;
      }
      /* END PERMUTATION, next permutation located in cities_to_permute */

    }//END COMPUTATIONAL LOOP
    #pragma omp barrier
  }//END PARALLELISM

  /* FINAL STEP: OUTPUT
   * Output final shortest path and distance.
   */
  printf("Best path: ");
  int j;
  for (j=0;j<num;j++)
    printf("%d ",shortest_path[j]);
  printf("\nDistance: %d\n",shortest_dist);
  //printf("Shortest path found by thread %d.\n",shortest_thread);

}

/* The following method was "stolen" from Lab1's provided framework. */
void get_input(char filename[])
{
  FILE * fp;
  int i,j;  

  fp = fopen(filename, "r");
  if(!fp)
  {
    printf("Cannot open file %s\n", filename);
    exit(1);
  }
 
  fscanf(fp,"%d ",&num);

 /* Now, time to allocate the matrices and vectors */
 a = (int**)malloc(num * sizeof(int*));
 if( !a)
  {
    printf("Cannot allocate a!\n");
    exit(1);
  }

 for(i = 0; i < num; i++) 
  {
    a[i] = (int *)malloc(num * sizeof(int)); 
    if( !a[i])
    {
        printf("Cannot allocate a[%d]!\n",i);
        exit(1);
    }
  }
 
 for(i = 0; i < num; i++)
 {
   for(j = 0; j < num; j++)
     fscanf(fp,"%d ",&a[i][j]);
 }
 
 fclose(fp); 

}