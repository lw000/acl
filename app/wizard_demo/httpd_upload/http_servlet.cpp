#include "stdafx.h"
#include "master_service.h"
#include "http_servlet.h"

http_servlet::http_servlet(acl::socket_stream* stream, acl::session* session)
: acl::HttpServlet(stream, session)
, read_body_(false)
, req_(NULL)
, res_(NULL)
, content_length_(0)
, read_length_(0)
, mime_(NULL)
{

}

http_servlet::~http_servlet(void)
{

}

bool http_servlet::doError(acl::HttpServletRequest&,
	acl::HttpServletResponse& res)
{
	res.setStatus(400);
	res.setContentType("text/html; charset=");
	// ���� http ��Ӧͷ
	if (res.sendHeader() == false)
		return false;

	// ���� http ��Ӧ��
	acl::string buf;
	buf.format("<root error='some error happened!' />\r\n");
	(void) res.getOutputStream().write(buf);
	return false;
}

bool http_servlet::doOther(acl::HttpServletRequest&,
	acl::HttpServletResponse& res, const char* method)
{
	res.setStatus(400);
	res.setContentType("text/html; charset=");
	// ���� http ��Ӧͷ
	if (res.sendHeader() == false)
		return false;
	// ���� http ��Ӧ��
	acl::string buf;
	buf.format("<root error='unkown request method %s' />\r\n", method);
	(void) res.getOutputStream().write(buf);
	return false;
}

bool http_servlet::run(void)
{
	if (read_body_ == false)
		return doRun();
	else if (req_ == NULL)
	{
		logger_error("req_ null");
		return false;
	}
	else if (res_ == NULL)
	{
		logger_error("res_ null");
		return false;
	}
	else
		return doBody(*req_, *res_);
}

bool http_servlet::doGet(acl::HttpServletRequest& req,
	acl::HttpServletResponse& res)
{
	return doPost(req, res);
}

bool http_servlet::doPost(acl::HttpServletRequest& req,
	acl::HttpServletResponse& res)
{
	res.setContentType("text/xml; charset=gbk")	// ������Ӧ�ַ���
		.setKeepAlive(req.isKeepAlive())	// �����Ƿ񱣳ֳ�����
		.setContentEncoding(true)		// �Զ�֧��ѹ������
		.setChunkedTransferEncoding(true);	// ���� chunk ���䷽ʽ

	// ��� HTTP ������������ͣ������Ĳ������ͣ��� name&value ��ʽ
	// ���� MIME �������ͣ���������������
	acl::http_request_t request_type = req.getRequestType();
	if (request_type != acl::HTTP_REQUEST_MULTIPART_FORM)
	{
		acl::string buf;
		buf.format("<root error='should acl::HTTP_REQUEST_MULTIPART_FORM' />\r\n");
		(void) res.write(buf);
		(void) res.write(NULL, 0);
		return false;
	}

	// �Ȼ�� Content-Type ��Ӧ�� http_ctype ����
	mime_ = req.getHttpMime();
	if (mime_ == NULL)
	{
		logger_error("http_mime null");
		(void) doReply(req, res, "http_mime null");
		return false;
	}

	// ���������ĳ���
	content_length_ = req.getContentLength();
	if (content_length_ <= 0)
	{
		logger_error("body empty");
		(void) doReply(req, res, "body empty");
		return false;
	}

	acl::string filepath;
#if defined(_WIN32) || defined(_WIN64)
	filepath.format("%s\\mime_file", var_cfg_var_path);
#else
	filepath.format("%s/mime_file", var_cfg_var_path);
#endif

	if (fp_.open_write(filepath) == false)
	{
		logger_error("open %s error %s",
			filepath.c_str(), acl::last_serror());
		(void) doReply(req, res, "open file error");
		return false;
	}

	// ����ԭʼ�ļ�����·��
	mime_->set_saved_path(filepath);

	req_ = &req;
	res_ = &res;
	read_body_ = true;

	// ֱ�ӷ��أ��Ӷ������첽�� HTTP ���������
	return true;
}

void http_servlet::reset(void)
{
	read_body_ = false;
	req_ = NULL;
	res_ = NULL;
	content_length_ = 0;
	read_length_ = 0;
	mime_ = NULL;
	fp_.close();
}

bool http_servlet::doBody(acl::HttpServletRequest& req,
	acl::HttpServletResponse& res)
{
	// ��δ����������ʱ����Ҫ�첽�� HTTP ����������
	if (content_length_ > read_length_)
	{
		if (doUpload(req, res) == false)
			return false;
	}

	if (content_length_ > read_length_)
		return true;

	// ���Ѿ����� HTTP ����������ʱ����ʼ�����ϴ�������
	bool ret = doParse(req, res);

	// ������ϣ������� HTTP �Ự״̬���Ա��ڴ�����һ�� HTTP ����
	reset();
	return ret;
}

bool http_servlet::doUpload(acl::HttpServletRequest& req,
	acl::HttpServletResponse& res)
{
	// ���������
	acl::istream& in = req.getInputStream();
	acl::string buf;
	bool  finish = false;

	//logger(">>>>>>>>>>read: %lld, total: %lld<<<<<",
	//	read_length_, content_length_);

	// ��ȡ HTTP �ͻ�����������
	while (content_length_ > read_length_)
	{
		if (in.read_peek(buf, true) == false)
			break;
		//if (buf.empty())
		//	break;
//		printf(">>>size: %ld, space: %ld\r\n",
//			(long) buf.size(), (long) buf.capacity());

		if (fp_.write(buf) == -1)
		{
			logger_error("write error %s", acl::last_serror());
			(void) doReply(req, res, "write error");
			return false;
		}

		read_length_ += buf.size();

		// �����õ����������������������н���
		if (!finish && mime_->update(buf, buf.size()) == true)
			finish = true;
	}

	if (in.eof())
	{
		logger_error("read error");
		return false;
	}

	return true;
}

bool http_servlet::doParse(acl::HttpServletRequest& req,
	acl::HttpServletResponse& res)
{
	const char* ptr = req.getParameter("name1");
	if (ptr)
		param1_ = ptr;
	ptr = req.getParameter("name2");
	if (ptr)
		param2_ = ptr;
	ptr = req.getParameter("name3");
	if (ptr)
		param3_ = ptr;

	acl::string path;

	// �������е� MIME ��㣬�ҳ�����Ϊ�ļ����Ĳ��ֽ���ת��
	const std::list<acl::http_mime_node*>& nodes = mime_->get_nodes();
	std::list<acl::http_mime_node*>::const_iterator cit = nodes.begin();
	for (; cit != nodes.end(); ++cit)
	{
		const char* name = (*cit)->get_name();
		if (name == NULL)
			continue;

		acl::http_mime_t mime_type = (*cit)->get_mime_type();
		if (mime_type == acl::HTTP_MIME_FILE)
		{
			const char* filename = (*cit)->get_filename();
			if (filename == NULL)
			{
				logger("filename null");
				continue;
			}

			// �е����������IE���ϴ��ļ�ʱ������ļ�·��������
			// ��Ҫ�Ƚ�·��ȥ��
			filename = acl_safe_basename(filename);
#if defined(_WIN32) || defined(_WIN64)
			path.format("%s\\%s", var_cfg_var_path, filename);
#else
			path.format("%s/%s", var_cfg_var_path, filename);
#endif
			(void) (*cit)->save(path.c_str());

			if (strcmp(name, "file1") == 0)
			{
				file1_ = filename;
				fsize1_ = get_fsize(var_cfg_var_path, filename);
			}
			else if (strcmp(name, "file2") == 0)
			{
				file2_ = filename;
				fsize2_ = get_fsize(var_cfg_var_path, filename);
			}
			else if (strcmp(name, "file3") == 0)
			{
				file3_ = filename;
				fsize3_ = get_fsize(var_cfg_var_path, filename);
			}
		}
	}

	// �������ص�ĳ���ļ���ת��
	const acl::http_mime_node* node = mime_->get_node("file1");
	if (node && node->get_mime_type() == acl::HTTP_MIME_FILE)
	{
		ptr = node->get_filename();
		if (ptr)
		{
			// �е����������IE���ϴ��ļ�ʱ������ļ�·��������
			// ��Ҫ�Ƚ�·��ȥ��
			ptr = acl_safe_basename(ptr);
#if defined(_WIN32) || defined(_WIN64)
			path.format("%s\\1_%s", var_cfg_var_path, ptr);
#else
			path.format("%s/1_%s", var_cfg_var_path, ptr);
#endif
			(void) node->save(path.c_str());
		}
	}

	return doReply(req, res, "OK");
}

bool http_servlet::doReply(acl::HttpServletRequest& req,
	acl::HttpServletResponse& res, const char* info)
{
	// ���� xml ��ʽ��������
	acl::xml1 body;

	body.get_root().add_child("root", true)
		.add_child("content_type", true)
			.add_attr("type", (int) req.getRequestType())
			.get_parent()
		.add_child("info", true)
			.set_text(info)
			.get_parent()
		.add_child("params", true)
			.add_child("param", true)
				.add_attr("name1", param1_)
			.get_parent()
			.add_child("param", true)
				.add_attr("name2", param2_)
			.get_parent()
			.add_child("param", true)
				.add_attr("name3", param3_)
			.get_parent()
		.add_child("files", true)
			.add_child("file", true)
				.add_attr("filename", file1_)
				.add_attr("fsize", fsize1_)
				.get_parent()
			.add_child("file", true)
				.add_attr("filename", file2_)
				.add_attr("fsize", fsize2_)
				.get_parent()
			.add_child("file", true)
				.add_attr("filename", file3_)
				.add_attr("fsize", fsize3_);
	acl::string buf;
	body.build_xml(buf);

	logger(">>%s<<", buf.c_str());
	return res.write(buf) && res.write(NULL, 0);
}

long long http_servlet::get_fsize(const char* dir, const char* filename)
{
	acl::string path;
#if defined(_WIN32) || defined(_WIN64)
	path.format("%s\\%s", dir, filename);
#else
	path.format("%s/%s", dir, filename);
#endif
	acl::ifstream in;
	if (in.open_read(path) == false)
	{
		logger_error("open %s error %s", path.c_str(), acl::last_serror());
		return -1;
	}
	return in.fsize();
}
