/*A table with all live transactions ids(client id) and its associated worker thread*/
/*Node structure of a file lock.A file is locked by a tranxaction in either shared mode(reading) or exclusive mode(writing)*/

#ifndef MY_HEADER_TX_
#define MY_HEADER_TX_

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <pthread.h>
#include <fcntl.h>
#include <dirent.h>
#include <libgen.h>
#include <fts.h>
#include <sys/time.h>
#include <stdbool.h>
#define NUM_OF_TXN 10000

struct file_lock
{
                        long file_inode;
                        int file_mode; // Permissions in case the file is created 
                        int fd;		/*file descriptor of the file for this particular transaction*/
			DIR *fd_dir;	/*holds the directory stream of an open directory.If the entry is that of a directory fd = -1*/
                        off_t file_offset;/*for this particular file descriptor*/
			long file_size;
                        int flags;	/*0-O_RDONLY,1-O_WRONLY,2-O_RDWR,3-O_APPEND*/
                        int parent_txn;
                        struct file_lock *next_txn;	   /*points to next locked file of same transaction*/
                        struct file_lock *prev_shared_rec;/*points to previous record of same file locked in compatible mode*/
                        struct file_lock *next_shared_rec;/*points to next record of same file locked in compatible mode*/
                        //struct txn_node *txn_wait;/*points to tranxaction waiting on this lock*/
                        /*If a file is held my multiple tranxactions in shared mode, every record of the file points to same wait list and once a writer transaction waits on a lock no shred mode lock is issued and a tid requesting a shared lock waits behind the writer*/
			struct file_lock *next;
			struct file_lock *prev;
//  In our design writes_at is in-effectual as all the data is updated in the memory itself 
			struct write_records *writes_at;	/*chronological list of writes to this file*/
// Delayed write = 1, data 
			bool delayed_write;
};

struct write_records
{
			long initial_off;
			long write_length;
/*used for write length in write syscalls. For rename/move it is used to indicate whetehr the file is old(-1) or new(-2)or old and new same(-3) mostly in "rename"*/
			long LRN;
			struct write_records *next;
			struct write_records *prev;
};

enum state {RUNNING = 1, WAITING, ABORTING, COMMIT};

struct txn_node{
                        int txn_id;
                        enum state cur_state;/*state of the transaction*/
                        long first_LRN;/*rec no. of the first log entry for this transaction*/
                        long last_LRN;/*latest rec no of log entry for this transaction*/

		// The structure file_lock will have to be changed ==> reflect the inode structure for YAFFS
                        struct file_lock *file_rec_no;/* pointer of first file it owns.We can get all files after reaching this*/

			struct txn_node *next;
                        /*THE NEXT THREE FIELDS ARE USED BY ONLY WAITING TRANXACTIONS*/
                        //struct txn_node *tid_next_wait;/*next tid wating for a file this tid is waiting*/
                        int wait_mode;/*0=waiting for a shared lock,1=exclusive lock,-1=not waiting*/
			long waiting_for_inode;/*the transaction is blocked while trying to access waiting_for_inode file*/			
                        int blocked_tid;/*If waiting,this holds the tid of waiting thread*/
			/*The before_after bit is used to serialize with the backup transaction*/
			int before_after;
                        };

enum log_type {TXN_BEG = 1, UPDATE,RENAME,CREAT,UNLINK,TXN_COMMIT,TXN_ABORT,TXN_END};

struct log_record{
			long LRN;/*Log Record NUmber, It is the starting byte value of the record or the offset of the record*/
			int txn_id;
			long prev_LRN;/*for a particular txn_id.First record is -1*/
			long next_LRN;/*initialized to -1 if only one record*/
			enum log_type record_type;
			/*other record fields for write or update records*/
			int fd;
			off_t offset;
			size_t write_length;
			char *data;/*for write all data to be written in one syscall is in data*/
			size_t write_length1;/*length of data in field data1*/
			char *data1;/*in case of Rename data holds the path of the source file and data1 holds the path of the destination*/
	};

/*If there is a waiting list against a file i.e transactions are waiting to access the file, threads wait on a per file condition variable. A waiting list with nodes of type below has a node each for files for which transactions are waiting*/ 
struct wait_node{
			long file_inode;
		  	int in_wait;/*number of transactions waiting on this file*/
			pthread_cond_t f_lock;
			/*the next four fields are used to syncronize with aborting due to deadlock*/
			int txnid_abort;/*this will hold the txn_id of the transaction to be aborted while resolving deadlock else it will hold -1*/
			int woke_up;/*number of threads that have woken up so far*/
			pthread_mutex_t wake_mutex;	
			pthread_cond_t wake_cond;/*to syncronize between waking up by deadlock thread or a fello user thread*/			
			struct wait_node *next;
		};

/*The following is a structure to store txnid of txns that are paused to serialize with the backup transactions. The structure shall hold the txnid of the paused txn and number of times it is oaused consecutively. The no. of times of nullified when the txn becomes MS with tb. This structure is used to abort a paused txn if it has to wait to more then a certain number of times which usually happens because of a deadlock scenario such as paused_txn waiting for 0 and 0 is waiting for say x and x is waiting for paused_txn. Thenumber of times to wait before abort can be adjusted according to the character of the trace. For exaple for localizede per txn trace, the number can be decreased.*/
struct pause_node{
		   int txnid;
		   int pause_times;
		};
struct pause_node pause_array[100];
int index_pause_array;

/*Declaring pointers to head of lists*/
struct file_lock *head_lock_list;
struct txn_node *head_txn_list;
struct wait_node *head_wait_list;

/*variable to hold the latest txn_id assigned*/
int txn_id;

/*Declaring mutexes*/
pthread_mutex_t  file_mutex,txn_mutex,log_mutex,txnid_mutex,pause_mutex;/*file_mutex is used to lock both the file list as well wait list(i.e the critical section between a consecutive lock and unlock command manipulates both file list as well as wait list) as it is only after accessing the file list we access the wait list*/

/*Declaring handler for the log file*/
FILE *event_log;

/*Declaring variable BACKUP which when = 1 means backup active else inactive*/
enum bck_state {INACTIVE,ACTIVE};

enum bck_state BACKUP;

/*Declaring functions*/
void print_lists(void);
struct file_lock *insert_file_list(long f_inode,char *file_path,int f_flags,int txn_id,int search_ret,struct file_lock *loc);
struct wait_node *search_wait_list(long file_inode);
int insert_wait_list(long f_inode,int txnid);
int update_wait_list(struct wait_node *loc_in_wait,int txnid);
void wake_up(struct wait_node *wait_list_loc);
void release_locks(int txn_id);
struct file_lock *resume(long f_inode,char *file_path,int f_flags,int txn_id);
void deadlock_detection(void);

/*Performance Measurement*/
int u_conflict;/*user serialization conflict*/
int tb_conflict;/*tb waiting for the release of a file by user transaction*/
int tbu_conflict;/*MS compromised*/
int no_of_abrt;/*no. of user transactions that has to be aborted because the transaction is not MS with tb*/
int no_on_wait;/*no. of transactions that waited to be MS with tb*/
int avg_wait_time;
int total_wait_time;
int abrt_after_pause;/*number of transactions that has to be aborted after pauseing for more then 3 times*/
int abrt_on_dead;/*number of transaction that needs to abort to recover from deadlock*/


#endif
