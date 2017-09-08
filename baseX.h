#pragma once

namespace DuiLib
{
	class CControlUI;
	class CContainerUI;
	class CPaintManagerUI;
}
namespace base{

	//���ü������ӿ�
	class IRef
	{
	public:
		virtual ~IRef() {  };
		virtual unsigned int AddRef() = 0;
		virtual unsigned int RelRef() = 0;
	};

	//���ü�����ʵ��
	template<typename theIBase >
	class Ref : public theIBase
	{
	public:
		Ref(bool bDel=true)
			:m_bDeleteThis(bDel)
		{
			m_unRef = 1; 
		}
		virtual ~Ref()
		{
		}
		inline unsigned int AddRef()
		{
			return InterlockedIncrement(&m_unRef);	
		}
		inline unsigned int RelRef()
		{	
			if( 0==InterlockedDecrement(&m_unRef) )
			{
				if( m_bDeleteThis )
				{
					delete this;
				}
				return 0;
			}
			return m_unRef;
		}
		inline unsigned int GetRef()
		{	return m_unRef;		}
	private:
		LONG volatile m_unRef;
		bool m_bDeleteThis;
	};
	//Ӧ�ó��ó�ʼ�����ͷ�
	class AppInitialize
	{
	public:
		AppInitialize()
		{
			WSAStartup(MAKEWORD(2,2),&wsaData);
			::CoInitialize(NULL);
			::OleInitialize(NULL);
		}
		virtual ~AppInitialize()
		{
			::OleUninitialize();
			::CoUninitialize();
			WSACleanup();
		}
		const WSADATA& GetSockStartData() const
		{
			return wsaData;
		}
	protected:
		WSADATA wsaData;
	};
	//Dll���� dll�ڼ���ʱ��Ҫ�����ù���Ŀ¼�����Ҫ������·��
	class DLL
	{
	public:
		//��Ҫ������·��
		DLL(LPCTSTR pDll) : m_dwError(0)
		{
			TCHAR chOld[MAX_PATH+1];
			TCHAR chMax[MAX_PATH+1];
			memset(chMax,0,sizeof(chMax));
			_tcscpy_s(chMax,MAX_PATH,pDll);
			::PathRemoveFileSpec(chMax);	//����dll·��
			::GetCurrentDirectory(MAX_PATH,chOld);
			::SetCurrentDirectory(chMax);
			m_hModule = ::LoadLibrary(pDll);
			if( m_hModule == NULL )
			{
				m_dwError = ::GetLastError();
			}
			::SetCurrentDirectory(chOld);
		}
		virtual ~DLL()
		{
			if( m_hModule )
			{
				FreeLibrary(m_hModule);
				m_hModule = NULL;
			}
		}
		template<typename F>
		F	FindFunction(const char *fun)
		{
			F ret;
			if( m_hModule == NULL )		return (F)0;
			ret = (F)(::GetProcAddress(m_hModule,fun));
			if( ret == 0 )
				m_dwError = ::GetLastError();
			return ret;
		}
		DWORD	GetLastError(bool bShow)
		{
			TCHAR chMsg[512];
			_stprintf_s(chMsg,512,_T("DLL LastError %d"),m_dwError);
			if( bShow )
			{
				::MessageBox(NULL,chMsg,_T("tip"),0);
			}
			return m_dwError;
		}
	protected:
		HMODULE	m_hModule;
		DWORD	m_dwError;
	};
	//duilib�������ͽ��нӿ�ת��
	template<typename theX>
	theX*	SafeConvert(DuiLib::CControlUI *pCtrl,LPCTSTR lpClass)
	{
		if( (pCtrl==NULL) || (lpClass==NULL) )
			return NULL;
		return static_cast<theX*>(pCtrl->GetInterface(lpClass));
	}
	template<typename theX>
	theX*	SafeConvert(DuiLib::CContainerUI *pCtrl,LPCTSTR lpName,LPCTSTR lpClass)
	{
		DuiLib::CControlUI *pRet = NULL;
		if( (pCtrl==NULL) || (lpClass==NULL) || (lpName==NULL) )
			return NULL;
		pRet = pCtrl->FindSubControl(lpName);
		if( pRet == NULL )	return NULL;
		return static_cast<theX*>(pRet->GetInterface(lpClass));
	}
	template<typename theX>
	theX*	SafeConvert(DuiLib::CPaintManagerUI *pm,LPCTSTR lpName,LPCTSTR lpClass)
	{
		DuiLib::CControlUI *pRet = NULL;
		if( (pm==NULL) || (lpName==0) || (lpClass==0) )
			return NULL;
		pRet = pm->FindControl(lpName);
		if( pRet == NULL )	return NULL;
		return static_cast<theX*>(pRet->GetInterface(lpClass));
	}
	/*---------------------------------------------------------------------*\
			�������ܣ�ö��ָ��·���µ������ļ�
			�������ƣ�EnumFolderFiles
			��	  ����pFolder �ļ���·��
			�� �� ֵ��
	\*---------------------------------------------------------------------*/
	template<class C>
	DWORD EnumFolderFiles(LPCTSTR pFolder,C *pC,bool (C::* pOne)(const ATL::CAtlString&,WIN32_FIND_DATA*))
	{
		WIN32_FIND_DATA	pData = { 0 };
		HANDLE hFind = INVALID_HANDLE_VALUE;	
		ATL::CAtlString strSpec,strPath(pFolder);
		strSpec.Format(_T("%s\\*.*"),pFolder);
		hFind = FindFirstFile(strSpec,&pData);
		if( hFind == INVALID_HANDLE_VALUE )
			return GetLastError();
		do 
		{
			if( false == (pC->*pOne)(strPath,&pData) )
				break;
		} while ( FindNextFile(hFind, &pData) );
		FindClose(hFind);
		hFind = INVALID_HANDLE_VALUE;
		return 0;
	}

	// bit_cast<Dest,Source> is a template function that implements the
	// equivalent of "*reinterpret_cast<Dest*>(&source)".  We need this in
	// very low-level functions like the protobuf library and fast math
	// support.
	//
	//   float f = 3.14159265358979;
	//   int i = bit_cast<int32>(f);
	//   // i = 0x40490fdb
	//
	// The classical address-casting method is:
	//
	//   // WRONG
	//   float f = 3.14159265358979;            // WRONG
	//   int i = * reinterpret_cast<int*>(&f);  // WRONG
	//
	// The address-casting method actually produces undefined behavior
	// according to ISO C++ specification section 3.10 -15 -.  Roughly, this
	// section says: if an object in memory has one type, and a program
	// accesses it with a different type, then the result is undefined
	// behavior for most values of "different type".
	//
	// This is true for any cast syntax, either *(int*)&f or
	// *reinterpret_cast<int*>(&f).  And it is particularly true for
	// conversions betweeen integral lvalues and floating-point lvalues.
	//
	// The purpose of 3.10 -15- is to allow optimizing compilers to assume
	// that expressions with different types refer to different memory.  gcc
	// 4.0.1 has an optimizer that takes advantage of this.  So a
	// non-conforming program quietly produces wildly incorrect output.
	//
	// The problem is not the use of reinterpret_cast.  The problem is type
	// punning: holding an object in memory of one type and reading its bits
	// back using a different type.
	//
	// The C++ standard is more subtle and complex than this, but that
	// is the basic idea.
	//
	// Anyways ...
	//
	// bit_cast<> calls memcpy() which is blessed by the standard,
	// especially by the example in section 3.9 .  Also, of course,
	// bit_cast<> wraps up the nasty logic in one place.
	//
	// Fortunately memcpy() is very fast.  In optimized mode, with a
	// constant size, gcc 2.95.3, gcc 4.0.1, and msvc 7.1 produce inline
	// code with the minimal amount of data movement.  On a 32-bit system,
	// memcpy(d,s,4) compiles to one load and one store, and memcpy(d,s,8)
	// compiles to two loads and two stores.
	//
	// WARNING: if Dest or Source is a non-POD type, the result of the memcpy
	// is likely to surprise you.
	template <class Dest, class Source>
	inline Dest bit_cast(const Source& source) 
	{
		// Compile time assertion: sizeof(Dest) == sizeof(Source)
		// A compile error here means your Dest and Source have different sizes.
		typedef char VerifySizesAreEqual [sizeof(Dest) == sizeof(Source) ? 1 : -1];
		Dest dest;
		memcpy(&dest, &source, sizeof(dest));
		return dest;
	}
}