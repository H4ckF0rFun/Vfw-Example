#include"VfwVideoCapture.h"

int main()
{
	VfwVideoCapture cap;
	cap.VfwStart(0,320 , 240);
	for (int i = 0; i < 100; i++)
	{
		PBYTE Buffer = NULL;
		DWORD dwBuffSize = 0;
		cap.VfwGetFrame(&Buffer,&dwBuffSize);
	}
}