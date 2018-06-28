#include <stdio.h>
#include <string.h>
#include <stdlib.h>

int main()
{
	char address[64];
	char memid[64];
	char user[64];
	char permissons[64];
	int step = 4;
	FILE *memOutput = popen ("ipcs", "r");

	char tmp[256];
	while( fscanf( memOutput, "%s", tmp) != EOF)
	{
		if( step == 4 && strncmp( tmp, "0x", 2 ) == 0 )
		{
			step = 1;
			strcpy( address, tmp);
			continue;
		}

		if( step == 1)
		{
			step = 2;
			strcpy( memid, tmp);
			continue;
		}

		if( step == 2)
		{
			step = 3;
			strcpy( user, tmp);
			continue;
		}

		if( step == 3)
		{
			step = 4;
			strcpy( permissons, tmp);

			char command[256];

			char userName[256];
			sprintf( userName, "%s", getenv("USER") );
			//printf("User Name : %s\n", userName );

			if( strncmp( user, userName, 5 ) == 0 && strncmp( permissons, "777", 3) == 0 )
			{
				sprintf ( command , "ipcrm -m %s >> /dev/null", memid );
				//printf( "%s\n", command );
				system( command );

				sprintf ( command , "ipcrm -s %s >> /dev/null", memid );
				system( command );

			}
			continue;
		}

	}

	pclose( memOutput );


}
