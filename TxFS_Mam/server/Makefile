all: TxFS_server

TxFS_server: Reader.c TxFS_manager.c Txn_manager.c Log_manager.c Lock_manager1.c Wait_list_manager.c Lock_manager2.c Update_manager.c backup_manager.c MS_manager.c deadlock.c
	gcc -o TxFS_server -g Reader.c TxFS_manager.c Txn_manager.c Log_manager.c Lock_manager1.c Wait_list_manager.c Lock_manager2.c Update_manager.c backup_manager.c MS_manager.c deadlock.c -L /btp/yaffs2-dir/TxFS_Mam/server -L. -pthread 
