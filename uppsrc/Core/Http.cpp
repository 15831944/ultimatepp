#include "Core.h"

NAMESPACE_UPP

namespace Ini {
	INI_BOOL(HttpRequest_Trace, false, "Activates HTTP requests tracing")
	INI_BOOL(HttpRequest_TraceBody, false, "Activates HTTP requests body tracing")
};

#define LLOG(x)      LOG_(Ini::HttpRequest_Trace, x)
#define LLOGB(x)     LOG_(Ini::HttpRequest_TraceBody, x)
	
#ifdef _DEBUG
_DBG_
// #define ENDZIP // only activate if zip pipe is in the question
#endif

void HttpRequest::Trace(bool b)
{
	Ini::HttpRequest_Trace = b;
	Ini::HttpRequest_TraceBody = b;
}

void HttpRequest::TraceHeader(bool b)
{
	Ini::HttpRequest_Trace = b;
}

void HttpRequest::TraceBody(bool b)
{
	Ini::HttpRequest_TraceBody = b;
}

void HttpRequest::Init()
{
	port = 0;
	proxy_port = 0;
	ssl_proxy_port = 0;
	max_header_size = 1000000;
	max_content_size = 10000000;
	max_redirects = 10;
	max_retries = 3;
	force_digest = false;
	std_headers = true;
	hasurlvar = false;
	keep_alive = false;
	method = METHOD_GET;
	phase = BEGIN;
	redirect_count = 0;
	retry_count = 0;
	gzip = false;
	WhenContent = callback(this, &HttpRequest::ContentOut);
	chunk = 4096;
	timeout = 120000;
	ssl = false;
}

HttpRequest::HttpRequest()
{
	Init();
}

HttpRequest::HttpRequest(const char *url)
{
	Init();
	Url(url);
}

HttpRequest& HttpRequest::Method(int m, const char *custom_name)
{
	method = m;
	custom_method = custom_name;
	return *this;
}

HttpRequest& HttpRequest::Url(const char *u)
{
	ssl = memcmp(u, "https", 5) == 0;
	const char *t = u;
	while(*t && *t != '?')
		if(*t++ == '/' && *t == '/') {
			u = ++t;
			break;
		}
	t = u;
	while(*u && *u != ':' && *u != '/' && *u != '?')
		u++;
	if(*u == '?' && u[1])
		hasurlvar = true;
	host = String(t, u);
	port = 0;
	if(*u == ':')
		port = ScanInt(u + 1, &u);
	path = u;
	int q = path.Find('#');
	if(q >= 0)
		path.Trim(q);
	return *this;
}

static
void sParseProxyUrl(const char *p, String& proxy_host, int& proxy_port)
{
	const char *t = p;
	while(*p && *p != ':')
		p++;
	proxy_host = String(t, p);
	if(*p++ == ':' && IsDigit(*p))
		proxy_port = ScanInt(p);
}

HttpRequest& HttpRequest::Proxy(const char *url)
{
	proxy_port = 80;
	sParseProxyUrl(url, proxy_host, proxy_port);
	return *this;
}

HttpRequest& HttpRequest::SSLProxy(const char *url)
{
	ssl_proxy_port = 8080;
	sParseProxyUrl(url, ssl_proxy_host, ssl_proxy_port);
	return *this;
}

HttpRequest& HttpRequest::Post(const char *id, const String& data)
{
	POST();
	if(postdata.GetCount())
		postdata << '&';
	postdata << id << '=' << UrlEncode(data);
	return *this;
}

HttpRequest& HttpRequest::Part(const char *id, const String& data,
                               const char *content_type, const char *filename)
{
	if(IsNull(multipart)) {
		POST();
		multipart = AsString(Uuid::Create());
		ContentType("multipart/form-data; boundary=" + multipart);
	}
	postdata << "--" << multipart << "\r\n"
	         << "Content-Disposition: form-data; name=\"" << id << "\"";
	if(filename && *filename)
	postdata << "; filename=\"" << filename << "\"";
	postdata << "\r\n";
	if(content_type && *content_type)
		postdata << "Content-Type: " << content_type << "\r\n";
	postdata << "\r\n" << data << "\r\n";
	return *this;
}

HttpRequest& HttpRequest::UrlVar(const char *id, const String& data)
{
	int c = *path.Last();
	if(hasurlvar && c != '&')
		path << '&';
	if(!hasurlvar && c != '?')
		path << '?';
	path << id << '=' << UrlEncode(data);
	hasurlvar = true;
	return *this;
}

String HttpRequest::CalculateDigest(const String& authenticate) const
{
	const char *p = authenticate;
	String realm, qop, nonce, opaque;
	while(*p) {
		if(!IsAlNum(*p)) {
			p++;
			continue;
		}
		else {
			const char *b = p;
			while(IsAlNum(*p))
				p++;
			String var = ToLower(String(b, p));
			String value;
			while(*p && (byte)*p <= ' ')
				p++;
			if(*p == '=') {
				p++;
				while(*p && (byte)*p <= ' ')
					p++;
				if(*p == '\"') {
					p++;
					while(*p && *p != '\"')
						if(*p != '\\' || *++p)
							value.Cat(*p++);
					if(*p == '\"')
						p++;
				}
				else {
					b = p;
					while(*p && *p != ',' && (byte)*p > ' ')
						p++;
					value = String(b, p);
				}
			}
			if(var == "realm")
				realm = value;
			else if(var == "qop")
				qop = value;
			else if(var == "nonce")
				nonce = value;
			else if(var == "opaque")
				opaque = value;
		}
	}
	String hv1, hv2;
	hv1 << username << ':' << realm << ':' << password;
	String ha1 = MD5String(hv1);
	hv2 << (method == METHOD_GET ? "GET" : method == METHOD_PUT ? "PUT" : method == METHOD_POST ? "POST" : "READ")
	<< ':' << path;
	String ha2 = MD5String(hv2);
	int nc = 1;
	String cnonce = FormatIntHex(Random(), 8);
	String hv;
	hv << ha1
	   << ':' << nonce
	   << ':' << FormatIntHex(nc, 8)
	   << ':' << cnonce
	   << ':' << qop << ':' << ha2;
	String ha = MD5String(hv);
	String auth;
	auth << "username=" << AsCString(username)
	     << ", realm=" << AsCString(realm)
	     << ", nonce=" << AsCString(nonce)
	     << ", uri=" << AsCString(path)
	     << ", qop=" << AsCString(qop)
	     << ", nc=" << AsCString(FormatIntHex(nc, 8))
	     << ", cnonce=" << cnonce
	     << ", response=" << AsCString(ha);
	if(!IsNull(opaque))
		auth << ", opaque=" << AsCString(opaque);
	return auth;
}

HttpRequest& HttpRequest::Header(const char *id, const String& data)
{
	request_headers << id << ": " << data << "\r\n";
	return *this;
}

HttpRequest& HttpRequest::Cookie(const HttpCookie& c)
{
	cookies.GetAdd(String(c.id).Cat() << '?' << c.domain << '?' << c.path) = c;
	return *this;
}

HttpRequest& HttpRequest::Cookie(const String& id, const String& value, const String& domain, const String& path)
{
	HttpCookie c;
	c.id = id;
	c.value = value;
	c.domain = domain;
	c.path = path;
	return Cookie(c);
}

HttpRequest& HttpRequest::CopyCookies(const HttpRequest& r)
{
	const HttpHeader& h = r.GetHttpHeader();
	for(int i = 0; i < h.cookies.GetCount(); i++)
		Cookie(h.cookies[i]);
	return *this;
}

void HttpRequest::HttpError(const char *s)
{
	if(IsError())
		return;
	error = NFormat(t_("%s:%d: ") + String(s), host, port);
	LLOG("HTTP ERROR: " << error);
	Close();
}

void HttpRequest::StartPhase(int s)
{
	phase = s;
	LLOG("Starting status " << s << " '" << GetPhaseName() << "', url: " << host);
	data.Clear();
}

void HttpRequest::New()
{
	ClearError();
	ClearAbort();
	phase = BEGIN;
}

bool HttpRequest::Do()
{
	int c1, c2;
	switch(phase) {
	case BEGIN:
		retry_count = 0;
		redirect_count = 0;
		start_time = msecs();
		GlobalTimeout(timeout);
	case START:
		Start();
		break;
	case DNS:
		Dns();
		break;
	case SSLPROXYREQUEST:
		if(SendingData())
			break;
		StartPhase(SSLPROXYRESPONSE);
		break;
	case SSLPROXYRESPONSE:
		if(ReadingHeader())
			break;
		ProcessSSLProxyResponse();
		break;
	case SSLHANDSHAKE:
		if(SSLHandshake())
			break;
		StartRequest();
		break;
	case REQUEST:
		if(SendingData())
			break;
		StartPhase(HEADER);
		break;
	case HEADER:
		if(ReadingHeader())
			break;
		StartBody();
		break;
	case BODY:
		if(ReadingBody())
			break;
		Finish();
		break;
	case CHUNK_HEADER:
		ReadingChunkHeader();
		break;
	case CHUNK_BODY:
		if(ReadingBody())
			break;
		c1 = TcpSocket::Get();
		c2 = TcpSocket::Get();
		if(c1 != '\r' || c2 != '\n')
			HttpError("missing ending CRLF in chunked transfer");
		StartPhase(CHUNK_HEADER);
		break;
	case TRAILER:
		if(ReadingHeader())
			break;
		header.ParseAdd(data);
		Finish();
		break;
	case FINISHED:
	case FAILED:
		WhenDo();
		return false;
	default:
		NEVER();
	}

	if(phase != FAILED)
		if(IsSocketError() || IsError())
			phase = FAILED;
		else
		if(msecs(start_time) >= timeout) {
			HttpError("connection timed out");
			phase = FAILED;
		}
		else
		if(IsAbort()) {
			HttpError("connection was aborted");
			phase = FAILED;
		}
	
	if(phase == FAILED) {
		if(retry_count++ < max_retries) {
			LLOG("HTTP retry on error " << GetErrorDesc());
			start_time = msecs();
			GlobalTimeout(timeout);
			StartPhase(START);
		}
	}
	WhenDo();
	return phase != FINISHED && phase != FAILED;
}

void HttpRequest::Start()
{
	LLOG("HTTP START");
	Close();
	ClearError();
	gzip = false;
	z.Clear();
	header.Clear();
	status_code = 0;
	reason_phrase.Clear();
	body.Clear();
	WhenStart();

	bool use_proxy = !IsNull(ssl ? ssl_proxy_host : proxy_host);

	int p = use_proxy ? (ssl ? ssl_proxy_port : proxy_port) : port;
	if(!p)
		p = ssl ? DEFAULT_HTTPS_PORT : DEFAULT_HTTP_PORT;
	String h = use_proxy ? ssl ? ssl_proxy_host : proxy_host : host;

	StartPhase(DNS);
	if(IsNull(GetTimeout())) {
		if(WhenWait) {
			addrinfo.Start(h, p);
			while(addrinfo.InProgress()) {
				Sleep(GetWaitStep());		
				WhenWait();
				if(msecs(start_time) >= timeout)
					break;
			}
		}
		else
			addrinfo.Execute(h, p);
		StartConnect();
	}
	else
		addrinfo.Start(h, p);
}

void HttpRequest::Dns()
{
	for(int i = 0; i <= Nvl(GetTimeout(), INT_MAX); i++) {
		if(!addrinfo.InProgress()) {
			StartConnect();
			return;
		}
		Sleep(1);
	}
}

void HttpRequest::StartConnect()
{
	LLOG("HTTP StartConnect");
	if(!Connect(addrinfo))
		return;
	if(ssl && ssl_proxy_host.GetCount()) {
		StartPhase(SSLPROXYREQUEST);
		String host_port = host;
		if(port)
			host_port << ':' << port;
		else
			host_port << ":443";
		data << "CONNECT " << host_port << " HTTP/1.1\r\n"
		     << "Host: " << host_port << "\r\n";
		if(!IsNull(ssl_proxy_username))
			data << "Proxy-Authorization: Basic "
			        << Base64Encode(proxy_username + ':' + proxy_password) << "\r\n";
		data << "\r\n";
		count = 0;
		LLOG("HTTPS proxy request:\n" << data);
	}
	else
		AfterConnect();
}

void HttpRequest::ProcessSSLProxyResponse()
{
	LLOG("HTTPS proxy response:\n" << data);
	int q = min(data.Find('\r'), data.Find('\n'));
	if(q >= 0)
		data.Trim(q);
	if(!data.StartsWith("HTTP") || data.Find(" 2") < 0) {
		HttpError("Invalid proxy reply: " + data);
		return;
	}
	AfterConnect();
}

void HttpRequest::AfterConnect()
{
	LLOG("HTTP AfterConnect");
	if(ssl && !StartSSL())
		return;
	if(ssl)
		StartPhase(SSLHANDSHAKE);
	else
		StartRequest();
}

void HttpRequest::StartRequest()
{
	StartPhase(REQUEST);
	count = 0;
	String ctype = contenttype;
	if((method == METHOD_POST || method == METHOD_PUT) && IsNull(ctype))
		ctype = "application/x-www-form-urlencoded";
	static const char *smethod[] = {
		"GET", "POST", "HEAD", "PUT", "DELETE", "TRACE", "OPTIONS", "CONNECT", "PATCH",
	};
	ASSERT(method >= 0 && method <= 8);
	data = Nvl(custom_method, smethod[method]);
	data << ' ';
	String host_port = host;
	if(port)
		host_port << ':' << port;
	String url;
	url << "http://" << host_port << Nvl(path, "/");
	if(!IsNull(proxy_host) && !ssl)
		data << url;
	else {
		if(*path != '/')
			data << '/';
		data << path;
	}
	data << " HTTP/1.1\r\n";
	String pd = postdata;
	if(!IsNull(multipart))
		pd << "--" << multipart << "--\r\n";
	if(method == METHOD_GET || method == METHOD_HEAD)
		pd.Clear();
	if(std_headers) {
		data << "URL: " << url << "\r\n"
		     << "Host: " << host_port << "\r\n"
		     << "Connection: " << (keep_alive ? "keep-alive\r\n" : "close\r\n") 
		     << "Accept: " << Nvl(accept, "*/*") << "\r\n"
		     << "Accept-Encoding: gzip\r\n"
		     << "User-Agent: " << Nvl(agent, "U++ HTTP request") << "\r\n";
		if(pd.GetCount() || method == METHOD_POST || method == METHOD_PUT)
			data << "Content-Length: " << pd.GetCount() << "\r\n";
		if(ctype.GetCount())
			data << "Content-Type: " << ctype << "\r\n";
	}
	VectorMap<String, Tuple2<String, int> > cms;
	for(int i = 0; i < cookies.GetCount(); i++) {
		const HttpCookie& c = cookies[i];
		if(host.EndsWith(c.domain) && path.StartsWith(c.path)) {
			Tuple2<String, int>& m = cms.GetAdd(c.id, MakeTuple(String(), -1));
			if(c.path.GetLength() > m.b) {
				m.a = c.value;
				m.b = c.path.GetLength();
			}
		}
	}
	String cs;
	for(int i = 0; i < cms.GetCount(); i++) {
		if(i)
			cs << "; ";
		cs << cms.GetKey(i) << '=' << cms[i].a;
	}
	if(cs.GetCount())
		data << "Cookie: " << cs << "\r\n";
	if(!IsNull(proxy_host) && !IsNull(proxy_username))
		 data << "Proxy-Authorization: Basic " << Base64Encode(proxy_username + ':' + proxy_password) << "\r\n";
	if(!IsNull(digest))
		data << "Authorization: Digest " << digest << "\r\n";
	else
	if(!force_digest && (!IsNull(username) || !IsNull(password)))
		data << "Authorization: Basic " << Base64Encode(username + ":" + password) << "\r\n";
	data << request_headers;
	LLOG("HTTP REQUEST " << host << ":" << port);
	LLOG("HTTP request:\n" << data);
	data << "\r\n" << pd;
	LLOGB("HTTP request body:\n" << pd);
}

bool HttpRequest::SendingData()
{
	for(;;) {
		int n = min(2048, data.GetLength() - (int)count);
		n = TcpSocket::Put(~data + count, n);
		if(n == 0)
			break;
		count += n;
	}
	return count < data.GetLength();
}

bool HttpRequest::ReadingHeader()
{
	for(;;) {
		int c = TcpSocket::Get();
		if(c < 0)
			return !IsEof();
		else
			data.Cat(c);
		if(data.GetCount() > 3) {
			const char *h = data.Last();
			if(h[0] == '\n' && (h[-1] == '\r' && h[-2] == '\n' || h[-1] == '\n'))
				return false;
		}
		if(data.GetCount() > max_header_size) {
			HttpError("HTTP header exceeded " + AsString(max_header_size));
			return true;
		}
	}
}

void HttpRequest::ReadingChunkHeader()
{
	for(;;) {
		int c = TcpSocket::Get();
		if(c < 0)
			break;
		else
		if(c == '\n') {
			int n = ScanInt(~data, NULL, 16);
			LLOG("HTTP Chunk header: 0x" << data << " = " << n);
			if(IsNull(n)) {
				HttpError("invalid chunk header");
				break;
			}
			if(n == 0) {
				StartPhase(TRAILER);
				break;
			}
			count += n;
			StartPhase(CHUNK_BODY);
			break;
		}
		if(c != '\r')
			data.Cat(c);
	}
}

String HttpRequest::GetRedirectUrl()
{
	String redirect_url = TrimLeft(header["location"]);
	if(redirect_url.StartsWith("http://") || redirect_url.StartsWith("https://"))
		return redirect_url;
	String h = (ssl ? "https://" : "http://") + host;
	if(*redirect_url != '/')
		h << '/';
	h << redirect_url;
	return h;
}

int64 HttpRequest::GetContentLength()
{
	int64 n = header.GetContentLength();
	return n < INT_MAX ? (int)n : 0;
}

void HttpRequest::StartBody()
{
	LLOG("HTTP Header received: ");
	LLOG(data);
	header.Clear();
	if(!header.Parse(data)) {
		HttpError("invalid HTTP header");
		return;
	}
	
	if(!header.Response(protocol, status_code, reason_phrase)) {
		HttpError("invalid HTTP response");
		return;
	}
	
	LLOG("HTTP status code: " << status_code);

	count = GetContentLength();
	
	if(method == METHOD_HEAD)
		phase = FINISHED;
	else
	if(header["transfer-encoding"] == "chunked") {
		count = 0;
		StartPhase(CHUNK_HEADER);
	}
	else
		StartPhase(BODY);
	body.Clear();
	gzip = GetHeader("content-encoding") == "gzip";
	if(gzip) {
		gzip = true;
		z.WhenOut = callback(this, &HttpRequest::Out);
		z.ChunkSize(chunk).GZip().Decompress();
	}
}

void HttpRequest::ContentOut(const void *ptr, int size)
{
	body.Cat((const char *)ptr, size);
}

void HttpRequest::Out(const void *ptr, int size)
{
	LLOG("HTTP Out " << size);
	if(z.IsError())
		HttpError("gzip format error");
	if(body.GetCount() + size > max_content_size) {
		HttpError("content length exceeded " + AsString(max_content_size));
		phase = FAILED;
		return;
	}
	WhenContent(ptr, size);
}

bool HttpRequest::ReadingBody()
{
	LLOG("HTTP reading body " << count);
	int n = chunk;
	if(count > 0)
		n = (int)min((int64)n, count);
	String s = TcpSocket::Get(n);
	if(s.GetCount() == 0)
		return !IsEof() && count;
#ifndef ENDZIP
	if(gzip)
		z.Put(~s, s.GetCount());
	else
#endif
		Out(~s, s.GetCount());
	if(count > 0) {
		count -= s.GetCount();
		return !IsEof() && count > 0;
	}
	return !IsEof();
}

void HttpRequest::CopyCookies()
{
	CopyCookies(*this);
}

void HttpRequest::Finish()
{
	if(gzip) {
	#ifdef ENDZIP
		body = GZDecompress(body);
		if(body.IsVoid()) {
			HttpError("gzip decompress at finish error");
			phase = FAILED;
			return;
		}
	#else
		z.End();
		if(z.IsError()) {
			HttpError("gzip format error (finish)");
			phase = FAILED;
			return;
		}
	#endif
	}
	Close();
	CopyCookies();
	if(status_code == 401 && !IsNull(username)) {
		String authenticate = header["www-authenticate"];
		if(authenticate.GetCount() && redirect_count++ < max_redirects) {
			LLOG("HTTP auth digest");
			Digest(CalculateDigest(authenticate));
			Start();
			return;
		}
	}
	if(status_code >= 300 && status_code < 400) {
		String url = GetRedirectUrl();
		GET();
		if(url.GetCount() && redirect_count++ < max_redirects) {
			LLOG("--- HTTP redirect " << url);
			Url(url);
			Start();
			retry_count = 0;
			return;
		}
	}
	phase = FINISHED;
}

String HttpRequest::Execute()
{
	New();
	while(Do())
		LLOG("HTTP Execute: " << GetPhaseName());
	return IsSuccess() ? GetContent() : String::GetVoid();
}

String HttpRequest::GetPhaseName() const
{
	static const char *m[] = {
		"Initial state",
		"Start",
		"Resolving host name",
		"SSL proxy request",
		"SSL proxy response",
		"SSL handshake",
		"Sending request",
		"Receiving header",
		"Receiving content",
		"Receiving chunk header",
		"Receiving content chunk",
		"Receiving trailer",
		"Request with continue",
		"Waiting for continue header",
		"Finished",
		"Failed",
	};
	return phase >= 0 && phase <= FAILED ? m[phase] : "";
}

END_UPP_NAMESPACE
