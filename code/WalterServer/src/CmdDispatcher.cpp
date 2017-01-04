/*
 * WebserverAPI.cpp
 *
 *  Created on: 26.12.2016
 *      Author: JochenAlt
 */

#include "CommDef.h"
#include "core.h"

#include "TrajectoryExecution.h"
#include "CmdDispatcher.h"
#include "logger.h"

#include "setup.h"
#include <vector>

CommandDispatcher commandDispatcher;


bool getURLParameter(vector<string> names, vector<string> values, string key, string &value) {
	for (int i = 0;i<(int)names.size();i++) {
		if (names[i].compare(key) == 0) {
			value = values[i];
			return true;
		}
	}
	return false;
}


void compileURLParameter(string uri, vector<string> &names, vector<string> &values) {
	names.clear();
	values.clear();

	std::istringstream iss(uri);
	std::string token;
	while (std::getline(iss, token, '&'))
	{
		// extract name and value of parameter
		int equalsIdx = token.find("=");
		if (equalsIdx > 0) {
			string name = token.substr(0,equalsIdx);
			string value = token.substr(equalsIdx+1);
			names.insert(names.end(), name);
			values.insert(values.end(), urlDecode(value));
		};
	}
}

CommandDispatcher::CommandDispatcher() {
	addCmdLine(">");
	addLogLine("start logging");
}

CommandDispatcher& CommandDispatcher::getInstance() {
	return commandDispatcher;
}

// returns true, if request has been dispatched
bool  CommandDispatcher::dispatch(string uri, string query, string body, string &response, bool &okOrNOk) {
	response = "";
	string urlPath = getPath(uri);

	vector<string> urlParamName;
	vector<string> urlParamValue;

	compileURLParameter(query,urlParamName,urlParamValue);

	// check if cortex command
	if (hasPrefix(uri, "/cortex")) {
		string cmd = uri.substr(string("/cortex/").length());
		for (int i = 0;i<CommDefType::NumberOfCommands;i++) {
			string cortexCmdStr = string(commDef[i].name);
			if (hasPrefix(cmd, cortexCmdStr)) {
				string command = string(commDef[i].name);
				// are there any parameters ?
				if (query.length() > 0) {
					std::istringstream iss(query);
					std::string token;
					while (std::getline(iss, token, '&'))
					{
						// extract name and value of parameter
						int equalsIdx = token.find("=");
						if (equalsIdx > 0) {
							string name = token.substr(0,equalsIdx);
							string value = token.substr(equalsIdx+1);
							command += " " + name + "=" + value;
						} else {
							command += " " + token;
						}
					}
				};
				LOG(DEBUG) << "calling cortex with \"" << command << "\"";
				string cmdReply;
				TrajectoryExecution::getInstance().directAccess(command, cmdReply, okOrNOk);

				if (cmdReply.length() > 0)
					response += cmdReply + "";
				if (okOrNOk)
					response += "ok.\r\n";
				else
					response += "failed.\r\n";
				return true;
			}
		}
	}

	if (hasPrefix(uri, "/direct")) {

		if (hasPrefix(query, "param=")) {
			string cmd = urlDecode(query.substr(string("param=").length()));
			LOG(DEBUG) << "calling cortex with \"" << cmd << "\"";
			string cmdReply;

			TrajectoryExecution::getInstance().directAccess(cmd, cmdReply, okOrNOk);
			std::ostringstream s;
			if (okOrNOk) {
				s << "OK";
			} else {
				s << "NOK(" << getLastError() << ") " << getErrorMessage(getLastError());
			}
			response += s.str();
			return true;
		}
	}

	if (hasPrefix(uri, "/executor/")) {
		string executorPath = uri.substr(string("/executor/").length());
		LOG(DEBUG) << uri;

		if (hasPrefix(executorPath, "startupbot")) {
			okOrNOk =  TrajectoryExecution::getInstance().startupBot();
			std::ostringstream s;
			if (okOrNOk) {
				s << "OK";
			} else {
				s << "NOK(" << getLastError() << ") " << getErrorMessage(getLastError());
			}
			response = s.str();
			return true;
		}
		else if (hasPrefix(executorPath, "teardownbot")) {
			okOrNOk = TrajectoryExecution::getInstance().teardownBot();
			std::ostringstream s;
			if (okOrNOk) {
				s << "OK";
			} else {
				s << "NOK(" << getLastError() << ") " << getErrorMessage(getLastError());
			}
			response = s.str();
			return true;
		}
		else if (hasPrefix(executorPath, "isupandrunning")) {
			response = "";
			bool result = TrajectoryExecution::getInstance().isBotUpAndReady();
			okOrNOk = true;
			response = result?"true":"false";
			return true;
		}
		else if (hasPrefix(executorPath, "setangles")) {
			string param = urlDecode(query.substr(string("param=").length()));
			okOrNOk = TrajectoryExecution::getInstance().setAnglesAsString(param);
			std::ostringstream s;
			if (okOrNOk) {
				s << "OK";
			} else {
				s << "NOK(" << getLastError() << ") " << getErrorMessage(getLastError());
			}
			response = s.str();
			return true;
		}
		else if (hasPrefix(executorPath, "getangles")) {
			response  = TrajectoryExecution::getInstance().currentTrajectoryNodeToString();
			okOrNOk = !isError();
			return true;
		}
		else if (hasPrefix(executorPath, "settrajectory")) {
			string param = urlDecode(body);
			TrajectoryExecution::getInstance().runTrajectory(param);
			okOrNOk = !isError();
			std::ostringstream s;
			if (okOrNOk) {
				s << "OK";
			} else {
				s << "NOK(" << getLastError() << ") " << getErrorMessage(getLastError());
			}
			response = s.str();
			return true;
		}
		else if (hasPrefix(executorPath, "stoptrajectory")) {
			TrajectoryExecution::getInstance().stopTrajectory();
			okOrNOk = !isError();
			std::ostringstream s;
			if (okOrNOk) {
				s << "OK";
			} else {
				s << "NOK(" << getLastError() << ") " << getErrorMessage(getLastError());
			}
			response = s.str();
			return true;
		}

	}

	if (hasPrefix(uri, "/web")) {
		string keyValue;
		if (getURLParameter(urlParamName, urlParamValue, "key", keyValue)) {
			response = getVariableJson(keyValue, okOrNOk);
			return okOrNOk;
		} else {
			if (getURLParameter(urlParamName, urlParamValue, "action", keyValue)) {
				if (keyValue.compare("savecmd") == 0) {
					string value;
					if (getURLParameter(urlParamName, urlParamValue, "value", value)) {
						string cmdReply;
						TrajectoryExecution::getInstance().directAccess(value, cmdReply, okOrNOk);
						response = cmdReply;
						return true;
					}
				}
			}
		}
	}

	okOrNOk = false;
	return false;
}


string CommandDispatcher::getVariableJson(string name, bool &ok) {
	ok = true;
	if (name.compare(string("cortexcmd")) == 0)
		return getCmdLineJson();;

	if (name.compare(string("cortexlog")) == 0)
		return getLogLineJson();

	if (name.compare(string("port")) == 0)
		return int_to_string(SERVER_PORT);
	ok = false;
	return string_format("variable named %s not found", name.c_str());
}


string  CommandDispatcher::getCmdLineJson() {
	string result = "[";
	result += cortexCmdJson + string(", ") + "{ \"line\":\"&gt\"}" + "]";
	return result;
}

string CommandDispatcher::getLogLineJson() {
	string result = "[ ";
	result += cortexLogJson + " ]";
	return result;
}

void CommandDispatcher::addCmdLine(string line) {
	int idx = cortexCmdJson.find("\"line\"");
	if (idx >= 0)
		cortexCmdJson += ", ";

	cortexCmdJson += "{ \"line\":\"" + htmlEncode(line) + "\"}";
}

void CommandDispatcher::addLogLine(string line) {
	int idx = cortexLogJson.find("\"line\"");

	if (idx >= 0)
		cortexLogJson += ", ";
	cortexLogJson += "{\"line\":\"" + htmlEncode(line) + "\"}";

	// remove staff from beginning if log gets loo long to be displayed
	while (cortexLogJson.length() > LOGVIEW_MAXSIZE) {
		int idx = cortexLogJson.find("\"line\"");
		if (idx >= 0) {
			idx = cortexLogJson.find("\"line\"", idx+1);
			if (idx>= 0) {
				cortexLogJson = cortexLogJson.substr(idx);
			}
		}
	}
}

