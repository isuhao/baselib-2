#include "stdafx.h"
#include "HttpClient.h"
namespace curl {
	static int dbg_trace(CURL *handle, curl_infotype type,char *data, size_t size,void *userp)
	{
		std::stringstream buf;
		switch (type) {
		case CURLINFO_TEXT:
			buf << data;
		default: /* in case a new one is introduced to shock us */
			return 0;
		case CURLINFO_HEADER_OUT:
			buf << "=> Send header";
			break;
		case CURLINFO_DATA_OUT:
			buf << "=> Send data";
			break;
		case CURLINFO_SSL_DATA_OUT:
			buf << "=> Send SSL data";
			break;
		case CURLINFO_HEADER_IN:
			buf << "<= Recv header";
			break;
		case CURLINFO_DATA_IN:
			buf << "<= Recv data";
			break;
		case CURLINFO_SSL_DATA_IN:
			buf << "<= Recv SSL data";
			break;
		}
		buf.write((char *)data, size);
		buf.flush();
		buf << "\r\n";
		::OutputDebugStringA(buf.str().c_str());
		return 0;
	}
	int StreamSeek(void *userp, curl_off_t offset, int origin) 
	{
		//if( offset==1 && origin== SEEK_SET)
		//	return CURL_SEEKFUNC_CANTSEEK;
		//std::stringstream *buf = static_cast<std::stringstream*>(userp);
		//buf->seekg(offset);
		return CURL_SEEKFUNC_CANTSEEK;
	}
	size_t StreamHeader(char *ptr, size_t size, size_t nmemb, void *userdata) 
	{
		std::stringstream *buf = static_cast<std::stringstream*>(userdata);
		if (size)
		{
			buf->write(ptr, size*nmemb);
			buf->flush();
		}
		if (buf->tellp() < 0)
			return 0;
		return (size*nmemb);
	}
	size_t StreamSave(char *ptr, size_t size, size_t nmemb, void *userdata)
	{
		std::stringstream *buf = static_cast<std::stringstream*>(userdata);
		if( size )
		{
			buf->write(ptr,size*nmemb);
			buf->flush();
		}
		if( buf->tellp() < 0 ) 
			return 0;
		return (size*nmemb);
	}
	size_t StreamSaveFile(char *ptr, size_t size, size_t nmemb, void *userdata)
	{
		FILE *buf = static_cast<FILE*>(userdata);
		size_t written = 0;
		if (size) {
			written = fwrite(ptr, size, nmemb, buf);
		}
		return (size*nmemb);
	}
	size_t StreamUpdate(void *ptr, size_t size, size_t nmemb, void *userdata)
	{
		std::stringstream *buf = static_cast<std::stringstream*>(userdata);
		std::string test = buf->str();
		buf->read((char*)ptr,size*nmemb);
		return buf->gcount();
	}
	void CHttpClient::GlobalSetup() 
	{
		curl_global_init(CURL_GLOBAL_ALL);
	}
	void CHttpClient::GlobalClean()
	{
		curl_global_cleanup();
	}
	long CHttpClient::PerformUrl(CURL* url)
	{
		CURLcode ret = curl_easy_perform(url);
		if (ret != CURLE_OK) {
			return ret;
		}
		long retcode = 0;
		ret = curl_easy_getinfo(url, CURLINFO_RESPONSE_CODE,&retcode);
		if (ret != CURLE_OK) {
			return ret;
		}
		return retcode;
	}
	CHttpClient::CHttpClient()
		:m_url(NULL)
		,m_headerlist(NULL)
		,m_postBoundary(NULL)
		,m_lastBoundary(NULL)
		,m_bWriteHeader(false)
		,m_Save2File(NULL)
		,m_bDecodeBody(false)
	{
		m_tmOut = 10000;
		m_tgProxy.nType = Proxy::NONE;
		m_url   = curl_easy_init();
	}
	void CHttpClient::SetContent(const std::string& data) 
	{
		m_rbuf << data;
		m_rbuf.flush();
	}
	const std::stringstream& CHttpClient::getContent() const {
		return m_rbuf;
	}
	CHttpClient::~CHttpClient()
	{
		if( m_url )
		{
			curl_easy_cleanup(m_url);
			m_url = NULL;
		}
		if( m_headerlist )
		{
			curl_slist_free_all(m_headerlist);
			m_headerlist=NULL;
		}
		ClearBoundary();
		m_header.clear();
	}
	void	CHttpClient::SetTimeout(long out)
	{
		m_tmOut = out;
	}
	void	CHttpClient::SetAgent(const CAtlString &val)
	{
		m_agent = val;
	}
	
	CURL*	CHttpClient::GetCURL()
	{
		return m_url;
	}
	void	CHttpClient::SetProxy(const Proxy &tgProxy)
	{
		m_tgProxy = tgProxy;
	}
	void	CHttpClient::ClearBoundary()
	{
		if( m_postBoundary)
		{
			curl_formfree(m_postBoundary);
			m_postBoundary = NULL;
		}
	}
	void	CHttpClient::EnableWriteHeader(bool b) 
	{
		m_bWriteHeader = b;
	}
	void	CHttpClient::AddBoundary(const std::string& szName, const std::string& szValue, ParamAttr dwParamAttr)
	{
		CURLFORMcode code = CURL_FORMADD_OK;
		if (dwParamAttr == ParamNormal)
		{
			code = curl_formadd(&m_postBoundary, &m_lastBoundary,
				CURLFORM_COPYNAME, szName.c_str(),
				CURLFORM_COPYCONTENTS, szValue.c_str(),
				CURLFORM_END);
		}
		else if (dwParamAttr == ParamFile)
		{
			code = curl_formadd(&m_postBoundary, &m_lastBoundary,
				CURLFORM_COPYNAME, szName.c_str(),
				CURLFORM_FILE, szValue.c_str(),
				CURLFORM_FILENAME, szName.c_str(),
				CURLFORM_END);
		}
		else if (dwParamAttr == ParamFileData) {
			code = curl_formadd(&m_postBoundary, &m_lastBoundary,
				CURLFORM_COPYNAME, "file",
				CURLFORM_FILENAME, szName.c_str(),
				CURLFORM_COPYCONTENTS, szValue.c_str(),
				CURLFORM_CONTENTSLENGTH,szValue.size(),
				CURLFORM_END);
		}
	}
	void	CHttpClient::AddBoundary(const CAtlString &szName,const CAtlString &szValue,ParamAttr dwParamAttr)
	{
		if( dwParamAttr==ParamNormal )
		{
			curl_formadd(&m_postBoundary,&m_lastBoundary,
						 CURLFORM_COPYNAME,(char*)CT2CA(szName),
						 CURLFORM_COPYCONTENTS,(char*)CT2CA(szValue),
						 CURLFORM_END);
		}
		else if( dwParamAttr==ParamFile )
		{
			std::string file;
			std::string name = (char*)CT2CA(szName, CP_UTF8);
			file = (char*)CT2CA(szValue, CP_UTF8);
			curl_formadd(&m_postBoundary, &m_lastBoundary,
				CURLFORM_COPYNAME, name.c_str(),
				CURLFORM_FILE, file.c_str(),
				CURLFORM_FILENAME, name.c_str(),
				CURLFORM_END);
		}
		else if (dwParamAttr == ParamFileData) {
			std::string val = (char*)CT2CA(szValue, CP_UTF8);
			std::string name = (char*)CT2CA(szName, CP_UTF8);
			curl_formadd(&m_postBoundary, &m_lastBoundary,
				CURLFORM_COPYNAME, "file",
				CURLFORM_FILENAME, name.c_str(),
				CURLFORM_COPYCONTENTS, val.c_str(),
				CURLFORM_END);
		}
	}
	void	CHttpClient::AddHeader(const CAtlString &szName, const CAtlString &szValue) {
		AddHeader((const char*)CT2CA(szName), (const char*)CT2CA(szValue));
	}
	void	CHttpClient::AddHeader(const std::string &szName, const std::string &szValue) {
		m_header.insert(std::make_pair(szName, szValue));
	}

	void	CHttpClient::AddParam(const CAtlString &szName, const CAtlString &szValue)
	{
		AddParam((const char*)CT2CA(szName),(const char*)CT2CA(szValue));
	}
	void	CHttpClient::AddParam(const std::string &szName, const std::string &szValue)
	{
		m_params.insert(std::make_pair(szName, szValue));
	}

	void	CHttpClient::BodySaveFile(FILE *f) 
	{
		m_Save2File = f;
	}
	void	CHttpClient::PerformParam(const std::string& url)
	{
		curl_easy_reset(m_url);
		m_wbuf.clear();	//���������ǣ��� ���������ָ��
		m_wbuf.str("");
		m_wbuf.flush();

		m_rbuf.clear();	//���������ǣ��� ���������ָ��

		m_headbuf.clear();
		m_headbuf.str("");
		{	//����Э��ͷ
			if (m_headerlist)
			{
				curl_slist_free_all(m_headerlist);
				m_headerlist = NULL;
			}
			mapStrings::iterator itPos = m_header.begin();
			for (itPos; itPos != m_header.end(); itPos++)
			{
				std::string strVal;
				if (!itPos->first.empty() && !itPos->second.empty()) {
					strVal = itPos->first + ":" + itPos->second;
					m_headerlist = curl_slist_append(m_headerlist,strVal.c_str());
				}				
			}
		}
		{
			//char *ret = curl_easy_escape(m_url,strTmp,strTmp.GetLength());
			//strTmp = ret;
			//curl_free(ret);
			curl_easy_setopt(m_url, CURLOPT_URL, url.c_str());
		}
		curl_easy_setopt(m_url, CURLOPT_CONNECTTIMEOUT, 10);		//���ӳ�ʱ
		if (m_headerlist)
			curl_easy_setopt(m_url, CURLOPT_HTTPHEADER, m_headerlist);
		if (!m_params.empty()) {
			std::string encode = encodeParam();
			curl_easy_setopt(m_url, CURLOPT_POSTFIELDSIZE, encode.length());
			curl_easy_setopt(m_url, CURLOPT_COPYPOSTFIELDS,encode.c_str());
		}
		if(!m_agent.IsEmpty())
			curl_easy_setopt(m_url, CURLOPT_USERAGENT, (char*)CT2CA(m_agent, CP_UTF8));
		else
			curl_easy_setopt(m_url, CURLOPT_USERAGENT, "Mozilla / 5.0");
		
		curl_easy_setopt(m_url, CURLOPT_TIMEOUT, m_tmOut / 100);	//��ʱ��λ��
		curl_easy_setopt(m_url, CURLOPT_FOLLOWLOCATION, 1L);
		curl_easy_setopt(m_url, CURLOPT_AUTOREFERER, 1L);
		if (!m_cookie.empty()) {
			curl_easy_setopt(m_url, CURLOPT_COOKIE,m_cookie.c_str());
		}
		if (m_bDecodeBody) {
			curl_easy_setopt(m_url, CURLOPT_HTTP_CONTENT_DECODING,1L);
			curl_easy_setopt(m_url, CURLOPT_HTTP_TRANSFER_DECODING,1L);
		}
		else {
			curl_easy_setopt(m_url, CURLOPT_HTTP_CONTENT_DECODING, 0L);
			curl_easy_setopt(m_url, CURLOPT_HTTP_TRANSFER_DECODING,0L);
		}

		//curl_easy_setopt(m_url, CURLOPT_COOKIESESSION, 1L);
		//curl_easy_setopt(m_url, CURLOPT_COOKIEFILE, "curl.cookie");
		//curl_easy_setopt(m_url, CURLOPT_ACCEPT_ENCODING, "zh-CN,zh;q=0.8");
		//keep alive
		//curl_easy_setopt(m_url, CURLOPT_TCP_KEEPALIVE, 1L);
		//curl_easy_setopt(m_url, CURLOPT_TCP_KEEPIDLE, 120L);
		//curl_easy_setopt(m_url, CURLOPT_TCP_KEEPINTVL, 60L);

		if (m_Save2File)
		{
			curl_easy_setopt(m_url, CURLOPT_WRITEFUNCTION, StreamSaveFile);
			curl_easy_setopt(m_url, CURLOPT_WRITEDATA, (void*)m_Save2File);
		}
		else
		{
			curl_easy_setopt(m_url, CURLOPT_WRITEFUNCTION, StreamSave);
			curl_easy_setopt(m_url, CURLOPT_WRITEDATA, (void*)&m_wbuf);
		}
		if (m_rbuf.tellp()) {
			curl_off_t nLen = (curl_off_t)m_rbuf.tellp();
			curl_easy_setopt(m_url, CURLOPT_READFUNCTION, StreamUpdate);
			curl_easy_setopt(m_url, CURLOPT_READDATA, (void*)&m_rbuf);
			/*
			curl_easy_setopt(m_url, CURLOPT_INFILESIZE, nLen);
			curl_easy_setopt(m_url, CURLOPT_INFILESIZE_LARGE, nLen);
			curl_easy_setopt(m_url, CURLOPT_PUT, 1L);
			curl_easy_setopt(m_url, CURLOPT_UPLOAD, 1L);
			*/
		}
		if (m_tgProxy.nType != Proxy::NONE)
		{
			curl_easy_setopt(m_url, CURLOPT_PROXYTYPE, m_tgProxy.nType);
			curl_easy_setopt(m_url, CURLOPT_PROXYPORT, m_tgProxy.nPort);
			curl_easy_setopt(m_url, CURLOPT_PROXY_SERVICE_NAME, (char*)CT2CA(m_tgProxy.strServer));
			curl_easy_setopt(m_url, CURLOPT_PROXYUSERNAME, (char*)CT2CA(m_tgProxy.strName));
			curl_easy_setopt(m_url, CURLOPT_PROXYUSERPWD, (char*)CT2CA(m_tgProxy.strPass));
		}
		if (m_postBoundary)
		{
			curl_easy_setopt(m_url, CURLOPT_HTTPPOST, m_postBoundary);
		}
		if (m_bWriteHeader) {
			curl_easy_setopt(m_url, CURLOPT_HEADERDATA, (void*)&m_headbuf);
			curl_easy_setopt(m_url, CURLOPT_HEADERFUNCTION, StreamHeader);
		}
		//curl_easy_setopt(m_url, CURLOPT_HTTP_VERSION, CURL_HTTP_VERSION_1_1);
#if defined(_DEBUG)
		curl_easy_setopt(m_url, CURLOPT_VERBOSE, 1L);
		curl_easy_setopt(m_url, CURLOPT_DEBUGFUNCTION, dbg_trace);
#endif
		m_header.clear();
		m_params.clear();
	}
	void	CHttpClient::EnableDecode(bool bDecode) 
	{
		m_bDecodeBody = bDecode;
	}
	void	CHttpClient::PerformParam(const CAtlString& url)
	{
		PerformParam((char*)CT2CA(url));		
	}
	std::string	CHttpClient::RequestPost(const CAtlString& url)
	{
		return RequestPost((char*)CT2CA(url));
	}
	std::string CHttpClient::RequestPost(const std::string& url) 
	{
		CURLcode code;
		if (m_url)
		{
			PerformParam(url);
			curl_easy_setopt(m_url, CURLOPT_POST, 1L);
			curl_easy_setopt(m_url, CURLOPT_POSTREDIR, CURL_REDIR_POST_ALL);
			if (m_rbuf.tellp() && m_params.empty()) {
				curl_easy_setopt(m_url, CURLOPT_POSTFIELDSIZE, m_rbuf.tellp());
			}
			code = curl_easy_perform(m_url);
		}
		return GetStream();
	}
	std::string	CHttpClient::GetStream()
	{
		std::string strRet("");
		if( m_wbuf.tellp() < 0 ) 
			return strRet;
		return m_wbuf.str();
	}
	std::string	CHttpClient::GetHeader()
	{
		std::string strRet("");
		if (m_headbuf.tellp() < 0)
			return strRet;
		return m_headbuf.str();
	}
	std::string CHttpClient::EncodeUrl(const std::string &v) 
	{
		std::string ret;
		char *result = curl_easy_escape(m_url, v.c_str(), v.length());
		if (result) {
			ret = result;
			curl_free(result);
		}
		return ret;
	}
	void	CHttpClient::SetCookie(const std::string &val) {
		m_cookie = val;
	}
	std::string CHttpClient::encodeParam() 
	{
		std::string ret;
		mapStrings::iterator itPos = m_params.begin();
		for (itPos; itPos != m_params.end(); itPos++)
		{
			std::string strVal;
			if (!itPos->first.empty() && !itPos->second.empty()) {
				char *key = curl_easy_escape(m_url,itPos->first.c_str(), itPos->first.length());
				char *val = curl_easy_escape(m_url,itPos->second.c_str(),itPos->second.length());
				if (key) {
					ret.append(key);
					curl_free(key);
				}
				else
					ret.append(itPos->first);
				ret.append("=");
				if (val) {
					ret.append(val);
					curl_free(val);
				}
				else
					ret.append(itPos->second);
				ret += "&";
			}
		}
		if (ret.empty()|| m_url==NULL)
			return ret;
		ret.erase( ret.begin()+(ret.length()-1) ); //ȥ������&
		return ret;
	}
}