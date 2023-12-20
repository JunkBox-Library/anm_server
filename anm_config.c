/**  
  Animation Relay Server: Configure File Tool Program 	

									anm_config.c v1.0 by Fumi.Iseki

*/



#include "anm_server.h"




//////////////////////////////////////////////////////////////////////////
// Configuration File
//

void  read_config_file(char* fn)
{
	tList* lt = read_index_tList_file(fn, ' ');

	if (lt!=NULL) {
		DEBUG_MODE   print_message("[%5d] READ_CONFIG_FILE: readed configuration file [%s]\n", CrntPID, fn);

		MinUdpDataPort   = get_intparam_tList(lt, "MinUdpDataPort",   MinUdpDataPort);
		MaxUdpDataPort   = get_intparam_tList(lt, "MaxUdpDataPort",   MaxUdpDataPort);
		MaxIdleTime	     = get_intparam_tList(lt, "MaxIdleTime",      MaxIdleTime);

		GrpKeyPrefix	 = get_strparam_tList(lt, "GroupKeyPrefix",   GrpKeyPrefix);
		Temp_File_Dir	 = get_strparam_tList(lt, "Temp_File_Dir",	  Temp_File_Dir);
		Hosts_Allow_File = get_strparam_tList(lt, "Hosts_Allow_File", Hosts_Allow_File);

		del_all_tList(&lt);
	}
	else {
		DEBUG_MODE print_message("[%5d] READ_CONFIG_FILE: cannot read configuration file [%s]\n", CrntPID, fn);
		DEBUG_MODE print_message("[%5d] READ_CONFIG_FILE: use default parameter\n", CrntPID);
	}


	DEBUG_MODE {
		print_message("[%5d] READ_CONFIG_FILE: MinUdpDataPort   = %d\n", CrntPID, MinUdpDataPort);
		print_message("[%5d] READ_CONFIG_FILE: MaxUdpDataPort   = %d\n", CrntPID, MaxUdpDataPort);
		print_message("[%5d] READ_CONFIG_FILE: MaxIdleTime      = %d\n", CrntPID, MaxIdleTime);
		print_message("[%5d] READ_CONFIG_FILE: Temp File Dir    = %s\n", CrntPID, Temp_File_Dir);
		print_message("[%5d] READ_CONFIG_FILE: Hosts Allow File = %s\n", CrntPID, Hosts_Allow_File);
		print_message("[%5d] READ_CONFIG_FILE: Group Key Prefix = %s\n", CrntPID, GrpKeyPrefix);
	}
}







