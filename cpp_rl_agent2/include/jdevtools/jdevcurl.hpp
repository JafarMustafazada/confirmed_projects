#ifndef JDEVTOOLS_JDEVCURL_HPP
#define JDEVTOOLS_JDEVCURL_HPP

#include <string>
#include <vector>
#include <stdexcept>

namespace {
#if defined(_WIN32)
#define popen _popen
#define pclose _pclose
#endif

	inline std::string exec(const char* cmd) {
		char buffer[128];
		std::string result = "";
		FILE* pipe = popen(cmd, "r");
		if (!pipe) throw std::runtime_error("popen() failed!");
		try {
			while (fgets(buffer, sizeof buffer, pipe) != NULL) {
				result += buffer;
			}
		} catch (...) {
			pclose(pipe);
			throw;
		}
		pclose(pipe);
		return result;
	}
}

namespace jdevtools {
	struct requestData {
		std::string url = "example.com";
		std::vector<std::string> headers;
		// url encoded data
		std::string postData = "";
		// data to be url encoded
		std::vector<std::string> urlEncodeData;
	};

	inline std::string sender(const requestData &req, bool isPost = false) {
		std::string command = "curl -s -o -";
		if (isPost) command += " -X POST \"" + req.url + '"';
		else command += " --location \"" + req.url + '"';
		for (int i = 0; i < req.headers.size(); i++) {
			command += " -H \"" + req.headers[i] + '"';
		}
		if (req.postData.size()) command += " -d \"" + req.postData + '"';
		for (int i = 0; i < req.urlEncodeData.size(); i++) {
			command += " --data-urlencode \"" + req.urlEncodeData[i] + '"';
		}
		// std::cout << command << '\n';
		return exec(command.data());
	}
}

#endif