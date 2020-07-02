/*
 * Driver code for airballoon problem
 */
#include <types.h>
#include <lib.h>
#include <thread.h>
#include <test.h>
#include <synch.h>

#define NROPES 16
static int ropes_left = NROPES;
static bool balloon_detached = false; //indicate if balloon has been detached or not
static bool actor_threads_complete = false; //indicates if all actors have finished their threads

/*----Data structures for rope mappings--------*/

//Balloon hook abstraction
struct balloon_hook{
    
    volatile int stake_index; //stake_index to which the hook is mapped to
    volatile bool is_mapped; //flag to indicate if mapped or not 


};

//Ground stake abstraction
struct ground_stake{
    
    //index i represents rope i, if true is the value at an any index i, Then that means that rope i is attached to it.
    volatile bool rope_mapping[NROPES];
    
};

//Rope abstraction
struct rope{
    
    
    volatile bool is_attached; //flag to indicate if rope is yet to be severed or not 
    
    struct lock *rope_lk; //Lock to protect the severing of ropes from both ends 
                             //and also swapping of ropes by LordFlowerKiller

};

//Represents all balloon hooks. Since the rope dont move across hooks,
//we can have a 1:1 mapping between the rope number and the hook number
struct balloon_hook balloon_hooks[NROPES];

//Represents all ground stakes. index i represents stake i
struct ground_stake ground_stakes[NROPES];

//Represents all ropes. index i represents rope i
struct rope ropes[NROPES];





/*-----------Synchronization primitives---------------*/

// Lock to ensure synchronised access of ropes_left variable
struct lock *ropes_left_lk;

//Semaphore used to block airballoon thread until each character
//finishes their respective threads
struct semaphore *exit_sem;

//CV used in order for the baloon thread to signal the main thread that it has finished 
struct cv *balloon_cv;
struct lock *cv_lock;

//CV used in order for the baloon thread to signal the main thread that it has finished 
struct cv *actor_cv;
struct lock *cv_lock_actor;

/*
 * Describe your design and any invariants or locking protocols 
 * that must be maintained. Explain the exit conditions. How
 * do all threads know when they are done?  
 */
 
 /*
 The basic idea of the design:
 
 - Each rope has a lock associated with it
 - The actor threads notify the main thread about when they have have finished by a semaphore mechanism
 - The condition variables have been used to communicate between the main thread and the baloon thread.
   One is to notify the balloon thread that all actors have finished their work.
   Second is to notify the main thread that baloon thread has finished it's work. 
   
 - The actor threads complete when no ropes are left to sever.
 - The baloon thread completes when all actor threads have finished 
  
 
*/
 
 

static
void
dandelion(void *p, unsigned long arg)
{
	(void)p;
	(void)arg;
	
    kprintf("Dandelion thread starting\n");
	
	//Local variables
	int rand_hook_index; // random hook index selected by Dandelion
	int stake_index; //stake index of the corresponding random hook index
	
	while (1) {

		//If no ropes left to severe, exit loop
       lock_acquire(ropes_left_lk);
        if ( ropes_left == 0) {	
        
			lock_release(ropes_left_lk);
			break;
		}
		lock_release(ropes_left_lk);
			
		// Random hook index generated
	    rand_hook_index = random() % NROPES;
	    
	    //If hook is not mapped start loop again
		if (!balloon_hooks[rand_hook_index].is_mapped) 
	       continue;
	
        /*------------------Aquiring rope lock---------------------*/
        lock_acquire(ropes[rand_hook_index].rope_lk);
	    
	       //stake_index rope is attached to
	       stake_index = balloon_hooks[rand_hook_index].stake_index;
		
           //If rope severed, release lock and continue loop
		   if (!ropes[rand_hook_index].is_attached) {
	
			 lock_release(ropes[rand_hook_index].rope_lk);
			  continue;

		   }
	
          //Actual severing of rope i.e removing mapping from each data structure
		  balloon_hooks[rand_hook_index].is_mapped = false;
		  ropes[rand_hook_index].is_attached = false;
	      ground_stakes[stake_index].rope_mapping[rand_hook_index] = false;
	
	      //Protecting the decrease of ropes_left 
          lock_acquire(ropes_left_lk);
             --ropes_left;
             kprintf("Dandelion severed rope %d\n", rand_hook_index);
          lock_release(ropes_left_lk);

	
		/*----------------Releasing rope lock----------------------*/	
		lock_release(ropes[rand_hook_index].rope_lk);	
		
		//Cause interleaving
	    thread_yield(); 
		
	
    }//close of while

    kprintf("Dandelion thread done\n");
   
    V(exit_sem); //signalling airballoon thread that thread is finished
    
    thread_exit();

	
}

static
void
marigold(void *p, unsigned long arg)
{
	(void)p;
	(void)arg;
	
	kprintf("Marigold thread starting\n");

	//Local variables
	int rand_stake_index; // random stake index selected by marigold
	int i; //index used for looping
	bool sever_successfull;
	
	while (1) {

		//If no ropes left to severe, exit loop
       lock_acquire(ropes_left_lk);
        if ( ropes_left == 0) {	
        
			lock_release(ropes_left_lk);
			break;
		}
		lock_release(ropes_left_lk);
			
		// Random stake index generated
	    rand_stake_index = random() % NROPES;
	    
	   
	  for(i = 0; i<NROPES ; i++){
	   
	     sever_successfull = true;
	    
	    //Getting first rope attcahed to the stake and severed it if not already severed  
	    if(ground_stakes[rand_stake_index].rope_mapping[i] == true && ropes[i].is_attached){
	   
	     
	     /*----------------------Aquiring rope lock-----------------------*/
        lock_acquire(ropes[i].rope_lk);
	      
	      //Checking if Lord FlowerKiller did not swap stakes before aquiring the lock
	       if(ground_stakes[rand_stake_index].rope_mapping[i] == false || !ropes[i].is_attached){ 
	       
	       sever_successfull = false;
	       lock_release(ropes[i].rope_lk);
	       break;
	       
	       }
	       
	      //Actual severing of rope i.e removing mapping from each data structure
		  balloon_hooks[i].is_mapped = false;
		  ropes[i].is_attached = false;
	      ground_stakes[rand_stake_index].rope_mapping[i] = false;
	
	      //Protecting the decrease of ropes_left 
          lock_acquire(ropes_left_lk);
             --ropes_left;
             kprintf("Marigold severed rope %d from stake %d\n", i, rand_stake_index);
          lock_release(ropes_left_lk);
            
       /*----------------Releasing rope lock----------------------*/	
		lock_release(ropes[i].rope_lk);	
		     
		     break;
		     
		  }//close of if
		  
	    }//close of for loop
		
		if(!sever_successfull)
		  continue;
		
		//Cause interleaving
	    thread_yield(); 
		
	
    }//close of while loop

    kprintf("Marigold thread done\n");
   
    V(exit_sem); //signalling airballoon thread that thread is finished
    
    thread_exit();
}

static
void
flowerkiller(void *p, unsigned long arg)
{
	(void)p;
	(void)arg;
	
	kprintf("Lord FlowerKiller thread starting\n");
	
	//Local variables
	int from_stake_index;
	int to_stake_index;
	int swap_successfull;
	int i;
	
	while (1) {

		//If no ropes left to swap
       lock_acquire(ropes_left_lk);
        if ( ropes_left == 0) {	
        
			lock_release(ropes_left_lk);
			break;
		}
		lock_release(ropes_left_lk);
		
		from_stake_index = random() % NROPES;
		to_stake_index = random() % NROPES;
		
		if(from_stake_index == to_stake_index)
		  continue;
		  
		  
	   for(i = 0; i<NROPES ; i++){
	   
	     swap_successfull = true;
	     
	     //Grabbing first rope attaached to stake and switching it if not severed 
	     if(ground_stakes[from_stake_index].rope_mapping[i] == true && ropes[i].is_attached){
	    
	    /*----------------------Aquiring rope lock-----------------------*/
        lock_acquire(ropes[i].rope_lk);
	      
	      //Checking if Dandelion or marigold severed the rope rope before aquiring the lock
	       if(ground_stakes[from_stake_index].rope_mapping[i] == false || !ropes[i].is_attached){ 
	       
	       swap_successfull = false;
	       lock_release(ropes[i].rope_lk);
	       break;
	       
	       }
	       
	      //Actual swapping of ropes
		  balloon_hooks[i].stake_index = to_stake_index;
		  ground_stakes[from_stake_index].rope_mapping[i] = false;
	      ground_stakes[to_stake_index].rope_mapping[i] = true;
	
	      kprintf("Lord FlowerKiller switched rope %d from stake %d to stake %d\n", i, from_stake_index, to_stake_index);
          
        /*----------------Releasing rope lock----------------------*/	
		lock_release(ropes[i].rope_lk);	
		     
		     break;
		     
		  }//close of if
		  
	    }//close of for loop
		
		
	    if(!swap_successfull)
		  continue;		
		
		
		//Cause interleaving
	    thread_yield(); 
		
	
    }//close of while loop

    kprintf("Lord FlowerKiller thread done\n");

    V(exit_sem); //signalling airballoon thread that thread is finished
    
    thread_exit();
}

static
void
balloon(void *p, unsigned long arg)
{
	(void)p;
	(void)arg;
	
	kprintf("Balloon thread starting\n");
	
	//Waiting for actor threads to complete
    lock_acquire(cv_lock_actor);
    
     while(!actor_threads_complete){
       cv_wait(actor_cv, cv_lock_actor);
     }
    lock_release(cv_lock_actor);
    
	
    //Signalling main thread
    lock_acquire(cv_lock);
    
     balloon_detached = true; 
     kprintf("Balloon freed and Prince Dandelion escapes!\n");
     kprintf("Balloon thread done\n");
    
     cv_signal(balloon_cv, cv_lock); //signalling main thread
    
    lock_release(cv_lock);

	thread_exit();
}


// Change this function as necessary
int
airballoon(int nargs, char **args)
{

	int err = 0;

	(void)nargs;
	(void)args;
	(void)ropes_left;
	int i;
	int j;
	
	ropes_left = NROPES;
	balloon_detached = false;
	actor_threads_complete = false;
	
	/*--------Initialize mapping of hook to stake i.e 1-1 mapping --------*/
	
	for(i = 0; i < NROPES ; i++){
	
	  for( j = 0; j < NROPES ; j++)
	     ground_stakes[i].rope_mapping[j] = false;
	  }
	
	for(i = 0; i < NROPES ; i++){
	
	 //Balloon hooks initialization
     balloon_hooks[i].stake_index = i;
     balloon_hooks[i].is_mapped = true;
	
	 //Ground stake initializion
     ground_stakes[i].rope_mapping[i] = true;
     
    }
	
	 // Initialization of ropes 
	 for(i = 0 ; i< NROPES ; i++){
	 
	   ropes[i].is_attached = true;
	   ropes[i].rope_lk = lock_create("rope_lk"); //creating locks
	   
	 }
	
	/*---------Initializing sync primitives---------------------------*/
	ropes_left_lk = lock_create("ropes_left_lk");
	exit_sem = sem_create("exit_sem", 0);
	balloon_cv = cv_create("balloon_cv");
	cv_lock = lock_create("cv_lock");
	actor_cv = cv_create("actor_cv");
	cv_lock_actor = lock_create("cv_lock_actor");
	
	/*------------Creating character threads ----------*/
	err = thread_fork("Marigold Thread",
			  NULL, marigold, NULL, 0);
	if(err)
		goto panic;
	
	err = thread_fork("Dandelion Thread",
			  NULL, dandelion, NULL, 0);
	if(err)
		goto panic;
	
	err = thread_fork("Lord FlowerKiller Thread",
			  NULL, flowerkiller, NULL, 0);
	if(err)
		goto panic;

	err = thread_fork("Air Balloon",
			  NULL, balloon, NULL, 0);
	if(err)
		goto panic;

	goto done;
panic:
	panic("airballoon: thread_fork failed: %s)\n",
	      strerror(err));
	
done: 
     
     //Waiting for dandelion, marigold, lord FlowerKiller thread to complete
     for (i = 0; i < 3; i++)
       P(exit_sem);
    
    //Signalling balloon thread
    lock_acquire(cv_lock_actor);
    
    actor_threads_complete = true; 
    cv_signal(actor_cv, cv_lock_actor); //signalling main thread
    
    lock_release(cv_lock_actor);
    
    //Waiting for balloon thread to complete
    lock_acquire(cv_lock);
    
    while(!balloon_detached){
       cv_wait(balloon_cv, cv_lock);
    }
    lock_release(cv_lock);
    
   
   //Memory cleanup
   ropes_left = NROPES;
   balloon_detached = false;
   actor_threads_complete = false;
   sem_destroy(exit_sem);
   lock_destroy(ropes_left_lk);
   lock_destroy(cv_lock);
   cv_destroy(balloon_cv);
   lock_destroy(cv_lock_actor);
   cv_destroy(actor_cv);
   
   for (i = 0; i < NROPES; ++i) {
	
		lock_destroy(ropes[i].rope_lk);	
	}
   

    kprintf("Main thread done\n");
    
	return 0;
}
