#pragma once
#include <unordered_map>
#include <vector>
#include <functional>
#include <sstream>

using Handler = std::function<void(const std::string&, const std::string&)>;

class MQTTDispatcher
{
public:
	void registerHandler(const std::string& topicPattern, const Handler handler)
	{
		handlers.push_back({split(topicPattern), handler});
	}

	void dispatch(const std::string& topic, const std::string& payload) const
	{
		auto topicLevels = split(topic);

		for (const auto& [patternLevels, handler] : handlers)
		{
			if (match(patternLevels, topicLevels))
			{
				handler(topic, payload);
			}
		}
	}

private:
	std::vector<std::pair<std::vector<std::string>, Handler>> handlers;

	static std::vector<std::string> split(const std::string& topic)
	{
		std::vector<std::string> result;
		std::stringstream ss(topic);
		std::string item;
		while (std::getline(ss, item, '/'))
		{
			result.push_back(item);
		}
		return result;
	}

	static bool match(const std::vector<std::string>& pattern, const std::vector<std::string>& topic)
	{
		size_t i = 0;
		for (; i < pattern.size(); ++i)
		{
			if (i >= topic.size())
			{
				return pattern[i] == "#";
			}

			if (pattern[i] == "#")
			{
				return true;
			}
			if (pattern[i] == "+")
			{
				continue;
			}
			if (pattern[i] != topic[i])
			{
				return false;
			}
		}

		return i == topic.size();
	}
};
