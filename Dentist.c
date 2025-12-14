#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <semaphore.h>

pthread_mutex_t dentist_mutex;
pthread_mutex_t appointment_mutex;
pthread_mutex_t walkin_mutex;
pthread_mutex_t finished_mutex;
pthread_mutex_t caller_mutex;

sem_t dentist_sem;
sem_t appointment_sem;
sem_t walkin_sem;
sem_t receptionist_sem;
sem_t* patient_sems;

int total_patients;
int patients_finished = 0;
int total_chairs;
int MAX_APPOINTMENTS;
int appointment_count = 0;
int dentist_state = 0;

int* walkin_queue;
int* appointment_queue;
int* caller_queue;

int walkin_rear = 0;
int appointment_rear = 0;
int caller_rear = 0;

void enqueue(int val, int* queue, int size, int* rear) {

    if (*rear == size)
    {
        return;
    }
    
    queue[(*rear)++] = val;

}

int dequeue(int* queue, int* rear) {

    if (*rear == 0)
    {
        return -1;
    }

    int val = queue[0];
    
    for (int i = 1; i < *rear; i++)
    {
        queue[i - 1] = queue[i];
    }
    
    (*rear)--;
    return val;

}

void* dentist(void* arg) {

    while (1)
    {
        pthread_mutex_lock(&finished_mutex);

        if (patients_finished == total_patients)
        {
            pthread_mutex_unlock(&finished_mutex);

            pthread_mutex_lock(&dentist_mutex);
            dentist_state = 0;
            pthread_mutex_unlock(&dentist_mutex);

            sem_post(&receptionist_sem);

            printf("Dentist has left the clinic\n");
            break;
        }

        pthread_mutex_unlock(&finished_mutex);

        pthread_mutex_lock(&appointment_mutex);
        pthread_mutex_lock(&walkin_mutex);

        int appointment_empty = (appointment_rear == 0);
        int walkin_empty = (walkin_rear == 0);

        pthread_mutex_unlock(&walkin_mutex);
        pthread_mutex_unlock(&appointment_mutex);

        if (appointment_empty && walkin_empty)
        {
            pthread_mutex_lock(&dentist_mutex);
            dentist_state = 0;
            pthread_mutex_unlock(&dentist_mutex);

            sem_wait(&dentist_sem);

            pthread_mutex_lock(&finished_mutex);
            if (patients_finished == total_patients) 
            {
                pthread_mutex_unlock(&finished_mutex);
                
                pthread_mutex_lock(&dentist_mutex);
                dentist_state = 0;
                pthread_mutex_unlock(&dentist_mutex);

                sem_post(&receptionist_sem);

                printf("Dentist has left the clinic\n");
                break;
            }
            pthread_mutex_unlock(&finished_mutex);

            pthread_mutex_lock(&dentist_mutex);
            dentist_state = 1;
            pthread_mutex_unlock(&dentist_mutex);

            continue;
        }

        pthread_mutex_lock(&appointment_mutex);

        if (appointment_rear != 0)
        {
            pthread_mutex_lock(&dentist_mutex);
            dentist_state = 2;
            pthread_mutex_unlock(&dentist_mutex);

            int id = dequeue(appointment_queue, &appointment_rear);
            pthread_mutex_unlock(&appointment_mutex);

            usleep((rand() % 1500 + 1000) * 1000);
            printf("Patient %d has been treated\n", id + 1);
            sem_post(&patient_sems[id]);

            pthread_mutex_lock(&dentist_mutex);
            dentist_state = 1;
            pthread_mutex_unlock(&dentist_mutex);

            continue;
        }
        
        pthread_mutex_unlock(&appointment_mutex);

        pthread_mutex_lock(&walkin_mutex);

        if (walkin_rear != 0)
        {
            pthread_mutex_lock(&dentist_mutex);
            dentist_state = 2;
            pthread_mutex_unlock(&dentist_mutex);

            int id = dequeue(walkin_queue, &walkin_rear);
            pthread_mutex_unlock(&walkin_mutex);

            usleep((rand() % 1500 + 1000) * 1000);
            printf("Patient %d has been treated\n", id + 1);
            sem_post(&patient_sems[id]);

            pthread_mutex_lock(&dentist_mutex);
            dentist_state = 1;
            pthread_mutex_unlock(&dentist_mutex);

            continue;
        }
        
        pthread_mutex_unlock(&walkin_mutex);
    }

    return NULL;

}

void* receptionist(void* arg) {

    while (1)
    {
        sem_wait(&receptionist_sem);

        pthread_mutex_lock(&caller_mutex);
        pthread_mutex_lock(&finished_mutex);

        int done = (patients_finished == total_patients);
        int empty = (caller_rear == 0);

        if (done && empty)
        {
            pthread_mutex_unlock(&finished_mutex);
            pthread_mutex_unlock(&caller_mutex);

            printf("Receptionist has left the clinic\n");
            break;
        }

        if (empty)
        {
            pthread_mutex_unlock(&finished_mutex);
            pthread_mutex_unlock(&caller_mutex);
            continue;
        }

        int patient_id = dequeue(caller_queue, &caller_rear);

        pthread_mutex_unlock(&finished_mutex);
        pthread_mutex_unlock(&caller_mutex);

        printf("Receptionist is checking if appointment is possible for Patient %d...\n", patient_id + 1);
        usleep((rand() % 4000 + 1000) * 1000);

        sem_post(&patient_sems[patient_id]);
    }

    return NULL;

}

void* walkin_patient(void* arg) {

    int id = (int)(long)arg;

    usleep((rand() % 2000) * 1000);
    printf("Patient %d has arrived at the clinic\n", id + 1);

    pthread_mutex_lock(&dentist_mutex);

    if (dentist_state == 0)
    {
        sem_post(&dentist_sem);
        dentist_state = 1;
    }

    pthread_mutex_unlock(&dentist_mutex);

    pthread_mutex_lock(&walkin_mutex);

    if (walkin_rear == total_chairs)
    {
        pthread_mutex_unlock(&walkin_mutex);

        pthread_mutex_lock(&finished_mutex);
        patients_finished++;

        if (patients_finished == total_patients) 
        {
            pthread_mutex_unlock(&finished_mutex);
            sem_post(&dentist_sem);
        } 
        else 
        {
            pthread_mutex_unlock(&finished_mutex);
        }

        printf("Patient %d has left the clinic without treatment\n", id + 1);
        return NULL;
    }

    enqueue(id, walkin_queue, total_chairs, &walkin_rear);
    pthread_mutex_unlock(&walkin_mutex);

    sem_wait(&patient_sems[id]);

    pthread_mutex_lock(&finished_mutex);
    patients_finished++;

    if (patients_finished == total_patients) 
    {
        pthread_mutex_unlock(&finished_mutex);
        sem_post(&dentist_sem);
    } 
    else 
    {
        pthread_mutex_unlock(&finished_mutex);
    }

    printf("Patient %d has left the clinic after treatment\n", id + 1);

    return NULL;

}

void* appointment_patient(void* arg) {

    int id = (int)(long)arg;

    pthread_mutex_lock(&caller_mutex);
    enqueue(id, caller_queue, total_patients, &caller_rear);
    pthread_mutex_unlock(&caller_mutex);

    printf("Patient %d is calling the receptionist for an appointment...\n", id + 1);
    sem_post(&receptionist_sem);

    sem_wait(&patient_sems[id]);

    pthread_mutex_lock(&appointment_mutex);

    if (appointment_count < MAX_APPOINTMENTS)
    {
        appointment_count++;
        pthread_mutex_unlock(&appointment_mutex);

        printf("Patient %d has made an appointment\n", id + 1);
    }
    else
    {
        pthread_mutex_unlock(&appointment_mutex);
        printf("Patient %d failed to make an appointment\n", id + 1);

        pthread_mutex_lock(&finished_mutex);
        patients_finished++;

        if (patients_finished == total_patients) 
        {
            pthread_mutex_unlock(&finished_mutex);
            sem_post(&dentist_sem);
        } 
        else 
        {
            pthread_mutex_unlock(&finished_mutex);
        }

        return NULL;
    }
    
    usleep((rand() % 3000 + 1000) * 1000);
    printf("Patient %d has arrived at the clinic\n", id + 1);

    pthread_mutex_lock(&dentist_mutex);

    if (dentist_state == 0)
    {
        sem_post(&dentist_sem);
        dentist_state = 1;
    }

    pthread_mutex_unlock(&dentist_mutex);

    pthread_mutex_lock(&appointment_mutex);
    enqueue(id, appointment_queue, MAX_APPOINTMENTS, &appointment_rear);
    pthread_mutex_unlock(&appointment_mutex);

    sem_wait(&patient_sems[id]);

    pthread_mutex_lock(&appointment_mutex);
    appointment_count--;
    pthread_mutex_unlock(&appointment_mutex);
    
    pthread_mutex_lock(&finished_mutex);
    patients_finished++;

    if (patients_finished == total_patients) 
    {
        pthread_mutex_unlock(&finished_mutex);
        sem_post(&dentist_sem);
    } 
    else 
    {
        pthread_mutex_unlock(&finished_mutex);
    }

    printf("Patient %d has left the clinic after treatment\n", id + 1);

    return NULL;

}

int main() {

    srand(time(NULL));

    printf("Enter number of patients: ");
    scanf("%d", &total_patients);
    printf("Enter number of chairs: ");
    scanf("%d", &total_chairs);
    printf("Enter limit of appointments: ");
    scanf("%d", &MAX_APPOINTMENTS);

    walkin_queue = malloc(sizeof(int) * total_chairs);
    appointment_queue = malloc(sizeof(int) * MAX_APPOINTMENTS);
    caller_queue = malloc(sizeof(int) * total_patients);
    patient_sems = malloc(sizeof(sem_t) * total_patients);

    pthread_t dentist_thread;
    pthread_t receptionist_thread;
    pthread_t patient_threads[total_patients];

    pthread_mutex_init(&dentist_mutex, NULL);
    pthread_mutex_init(&walkin_mutex, NULL);
    pthread_mutex_init(&appointment_mutex, NULL);
    pthread_mutex_init(&finished_mutex, NULL);

    sem_init(&dentist_sem, 0, 0);
    sem_init(&walkin_sem, 0, 0);
    sem_init(&appointment_sem, 0, 0);
    sem_init(&receptionist_sem, 0, 0);

    pthread_create(&dentist_thread, NULL, dentist, NULL);
    pthread_create(&receptionist_thread, NULL, receptionist, NULL);

    for (int i = 0; i < total_patients; i++)
    {
        sem_init(&patient_sems[i], 0, 0);

        int patient_type = rand() % 2;

        if (patient_type == 0)
        {
            pthread_create(&patient_threads[i], NULL, walkin_patient, (void*)(long)(i));
        }
        else
        {
            pthread_create(&patient_threads[i], NULL, appointment_patient, (void*)(long)(i));
        } 
    }

    pthread_join(dentist_thread, NULL);
    pthread_join(receptionist_thread, NULL);
    
    for (int i = 0; i < total_patients; i++)
    {
        pthread_join(patient_threads[i], NULL);
    }

    pthread_mutex_destroy(&dentist_mutex);
    pthread_mutex_destroy(&walkin_mutex);
    pthread_mutex_destroy(&appointment_mutex);
    pthread_mutex_destroy(&finished_mutex);

    sem_destroy(&dentist_sem);
    sem_destroy(&walkin_sem);
    sem_destroy(&appointment_sem);
    sem_destroy(&receptionist_sem);

    free(walkin_queue);
    free(appointment_queue);
    free(caller_queue);
    free(patient_sems);

    return 0;

}
