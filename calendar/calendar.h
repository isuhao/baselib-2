// calendar.cpp : �������̨Ӧ�ó������ڵ㡣
//

#include <atltime.h>
namespace base{

	class Calendar : public ATL::CTime
	{
	public:
		Calendar() throw();
		Calendar( __time64_t time ) throw();
		Calendar( const ATL::CTime & tm ) throw();
		Calendar( int nYear, int nMonth, int nDay, int nHour, int nMin, int nSec,int nDST = -1 );
		Calendar( WORD wDosDate, WORD wDosTime, int nDST = -1 );
		Calendar( const SYSTEMTIME& st, int nDST = -1 );
		Calendar( const FILETIME& ft, int nDST = -1 );
		//��ȡ�й�ϰ�ߵ����ڼ�
		//��һ 1 ���� ���� 7
		int GetDayOfWeekByChina();
		bool IsValid();
		Calendar& operator=( const ATL::CTime& time );
		Calendar& operator=( const Calendar& time);
		bool PlusYear(int v);
		bool PlusMonth(int v);
	public:
		//��ȡ��ǰ������
		int GetMonthDays();
		//��ȡĳ��ĳ������
		static int GetMonthDays(int y,int m);
		//���ڻ�ķ����ɭ���㹫ʽ
		//@param y ��� �磺1990��
		//@param m �·� �磺1��
		//@param d ���� �磺20��
		static int DayOfWeek(int y,int m,int d);
	};
}