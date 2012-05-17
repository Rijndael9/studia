#include <ncurses.h>
#include <pthread.h>
#include <time.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>

#define MICRO 1000000
#define PITSTOP_TIME 9
#define LAP_LENGTH 40
#define PS_X 22
#define PS_Y 0
#define MAX_X 19
#define MAX_Y 9

struct thread_data_t{
  int id;
  int speed;
  int fuel_laps;
  int current_fuel;
} ;

pthread_mutex_t refresh_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t pitstop_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t paint_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t cars_mutex = PTHREAD_MUTEX_INITIALIZER;

pthread_cond_t pitstop_cond = PTHREAD_COND_INITIALIZER;
pthread_cond_t paint_cond = PTHREAD_COND_INITIALIZER;


int lapLength, threadsCount, rc, restCount;
bool pitstop = false;
bool repaint = true;
bool finish = false;
pthread_t * threads;
pthread_t paint_thread;
thread_data_t * threadsParameters;

char lap[11][24];
int ** cars;


//===============================================
//Functions headers
void *car_thread_function(void *arg);
void *paint_thread_function(void *arg);
void init_threads();
void clear_car_threads();
void clear_paint_thread();
void clear_rest();
void print_lap();

//===============================================
int main(){
    init_threads();
    clear_car_threads();
    clear_paint_thread();
    clear_rest();
    return 0;
}

//===============================================
void init_threads() {
    srand(time(NULL));
    initscr();
    printw("Lap length : \n");
    scanw("%d", &lapLength);

    printw("Threads count : \n");
    scanw("%d", &threadsCount);

    restCount = threadsCount;
    threads = new pthread_t[threadsCount];


    threadsParameters = new thread_data_t[threadsCount];


    cars = new int*[threadsCount];
    for(int i=0; i<threadsCount; i++){
        cars[i] = new int[2];
        cars[i][0] = MAX_X;
        cars[i][1] = 0;
    }

    for(int i=0; i<threadsCount; i++){
        printw("Thread %d speed(1-10) and fuel \n", i+1);
        refresh();
        threadsParameters[i].id = i+1;
        scanw("%d %d", &threadsParameters[i].speed, &threadsParameters[i].fuel_laps);
    }

    refresh();
    
    rc = pthread_create(&paint_thread, NULL, paint_thread_function, NULL);
    for(int i=0; i<threadsCount; i++) {
        if((rc = pthread_create(&threads[i], NULL, car_thread_function, &threadsParameters[i]))){
            printw("Thread %d init error \n", i) ;
        }
    }   
}

void clear_car_threads(){
    for (int i=0; i<threadsCount; i++) {
        pthread_join(threads[i], NULL);
    }   
    finish = true;
}

void clear_paint_thread(){
    pthread_join(paint_thread, NULL);  
}

//===============================================
void clear_rest() {

    pthread_mutex_destroy(&pitstop_mutex);
    pthread_mutex_destroy(&cars_mutex);
    pthread_mutex_destroy(&paint_mutex);
    pthread_mutex_destroy(&cars_mutex);

    pthread_cond_destroy(&paint_cond);
    pthread_cond_destroy(&pitstop_cond);

    delete [] threads;
    delete [] threadsParameters;

    for(int i=0; i<threadsCount; i++){
        delete [] cars[i];
    }
    delete [] cars;

    printw("press any key\n");
    getch();
    endwin();
}

//===============================================
void *car_thread_function(void *arg) {

    thread_data_t *data = (thread_data_t *)arg;
    int fuel = data->fuel_laps;
    int loops = 1;
    int sleep = (lapLength * MICRO / data->speed) / LAP_LENGTH;
    int x = MAX_X; 
    int y = 0;
    int cid = data->id;

    while(fuel > 0 && restCount > 1){
        usleep(sleep);

        if(x > 0 && y == 0) { x--; }
        else if(x == 0 && y < 9) { y++; }
        else if(x < 19 && y == 9) { x++; }
        else if(x == 19 && y > 0 ) { y--; }

        if(x == 19 && y ==0) {
            loops++;
            fuel--;
            
            pthread_mutex_lock(&pitstop_mutex);
            if(!pitstop) {
                
                pitstop = true;
                pthread_cond_signal(&pitstop_cond);
                pthread_mutex_unlock(&pitstop_mutex);


                pthread_mutex_lock(&paint_mutex);
                cars[cid-1][0] = 21;
                cars[cid-1][1] = 0;
                repaint = true;
                pthread_cond_signal(&paint_cond);
                pthread_mutex_unlock(&paint_mutex);

                usleep(((rand() % PITSTOP_TIME) +1) * MICRO);
                fuel = data->fuel_laps;

                pthread_mutex_lock(&pitstop_mutex);
                pitstop = false;    
                pthread_cond_signal(&pitstop_cond);
                pthread_mutex_unlock(&pitstop_mutex);
            } else {
                pthread_mutex_unlock(&pitstop_mutex);
            }


        } else {
            pthread_mutex_lock(&paint_mutex);
            cars[cid-1][0] = x;
            cars[cid-1][1] = y;
            repaint = true;
            pthread_cond_signal(&paint_cond);
            pthread_mutex_unlock(&paint_mutex);            
        }

        

    }
    
    pthread_mutex_lock(&cars_mutex);
    if(restCount > 1) {
        restCount--;
        pthread_mutex_unlock(&cars_mutex);

        pthread_mutex_lock(&paint_mutex);
        cars[cid-1][0] = -1;
        cars[cid-1][1] = -1;
        repaint = true;
        pthread_cond_signal(&paint_cond);
        pthread_mutex_unlock(&paint_mutex);
    } else {
        pthread_mutex_unlock(&cars_mutex);

        pthread_mutex_lock(&paint_mutex);
        repaint = true;
        pthread_cond_signal(&paint_cond);
        pthread_mutex_unlock(&paint_mutex);        
    }

    pthread_exit(NULL);
}

//===============================================
void *paint_thread_function(void *arg){
    while(!finish){
        pthread_mutex_lock(&cars_mutex);
        if(restCount > 1) {
            pthread_mutex_unlock(&cars_mutex);
            pthread_mutex_lock(&paint_mutex);
            while(!repaint) pthread_cond_wait(&paint_cond, &paint_mutex);
            print_lap();
            repaint = false;
            pthread_mutex_unlock(&paint_mutex);
        } else {
            pthread_mutex_unlock(&cars_mutex);
        }
    }

    for(int i=0; i<threadsCount; i++){
        if(cars[i][0] != -1 && cars[i][1] != -1) {
            printw("Car %d win!!!! \n", i+1);
            refresh();
            break;
        }
    }


    pthread_exit(NULL);
}

//===============================================
void print_lap(){
    clear();
    strcpy(lap[0], "--------------------[ ]");
    for(int i=1; i<9;i++){
        strcpy(lap[i], "|                  |");
    }
    strcpy(lap[9], "--------------------");

    for(int i=0; i<threadsCount; i++){
        if(cars[i][0] != -1 && cars[i][1] != -1) {
            lap[cars[i][1]][cars[i][0]] = (char)(49 + i);
        }
    }

    for(int i=0; i<10; i++){
        printw("%s\n",lap[i]);
    }
    refresh();

}