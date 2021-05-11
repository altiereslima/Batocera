#include "ResourceManager.h"

#include "utils/FileSystemUtil.h"
#include "utils/StringUtil.h"
#include <fstream>

auto array_deleter = [](unsigned char* p) { delete[] p; };
auto nop_deleter = [](unsigned char* /*p*/) { };

std::shared_ptr<ResourceManager> ResourceManager::sInstance = nullptr;

ResourceManager::ResourceManager()
{
}

std::shared_ptr<ResourceManager>& ResourceManager::getInstance()
{
	if(!sInstance)
		sInstance = std::shared_ptr<ResourceManager>(new ResourceManager());

	return sInstance;
}

std::string ResourceManager::getResourcePath(const std::string& path) const
{
	// check if this is a resource file
	if (path.size() < 2 || path[0] != ':' || path[1] != '/')
		return path;

	// check in homepath
	std::string test = Utils::FileSystem::getEsConfigPath() + "/resources/" + &path[2];
	if(Utils::FileSystem::exists(test))
		return test;

	
#if WIN32 && _DEBUG
	// Windows DEBUG -> Avoid resource & locale copy -> Look in project directories
	
	test = Utils::String::replace(Utils::FileSystem::getSharedConfigPath(), "/Debug", "") + "/resources/" + &path[2];
	if (Utils::FileSystem::exists(test))
		return test;

	if (path.find(":/locale/") != std::string::npos)
	{
		test = Utils::String::replace(Utils::FileSystem::getSharedConfigPath(), "/Debug", "") + "/" + &path[2];
		if (Utils::FileSystem::exists(test))
			return test;

		test = Utils::String::replace(Utils::FileSystem::getSharedConfigPath(), "/Debug", "") + "/" + Utils::String::replace(path, ":/locale/", "locale/lang/");
		if (Utils::FileSystem::exists(test))
			return test;
	}

#endif

	// check in exepath
	test = Utils::FileSystem::getSharedConfigPath() + "/resources/" + &path[2];
	if(Utils::FileSystem::exists(test))
		return test;

	// check in cwd
	test = Utils::FileSystem::getCWDPath() + "/resources/" + &path[2];
	if(Utils::FileSystem::exists(test))
		return test;

	// not a resource, return unmodified path
	return path;
}

const ResourceData ResourceManager::getFileData(const std::string& path) const
{
	//check if its a resource
	const std::string respath = getResourcePath(path);

	auto size = Utils::FileSystem::getFileSize(respath);
	if (size > 0)
	{
		ResourceData data = loadFile(respath, size);
		return data;
	}

	//if the file doesn't exist, return an "empty" ResourceData
	ResourceData data = {NULL, 0};
	return data;
}

ResourceData ResourceManager::loadFile(const std::string& path, size_t size) const
{
	std::ifstream stream(path, std::ios::binary);
	
	if (size == 0 || size == SIZE_MAX)
	{
		stream.seekg(0, stream.end);
		size = (size_t)stream.tellg();
		stream.seekg(0, stream.beg);
	}

	//supply custom deleter to properly free array
	std::shared_ptr<unsigned char> data(new unsigned char[size], array_deleter);
	stream.read((char*)data.get(), size);
	stream.close();

	ResourceData ret = {data, size};
	return ret;
}

bool ResourceManager::fileExists(const std::string& path) const
{
	//if it exists as a resource file, return true
	if(getResourcePath(path) != path)
		return true;

	return Utils::FileSystem::exists(path);
}

void ResourceManager::unloadAll()
{
	auto iter = mReloadables.cbegin();
	while(iter != mReloadables.cend())
	{
		std::shared_ptr<ReloadableInfo> info = *iter;

		if (!info->data.expired())
		{
			info->reload = info->data.lock()->unload();
			iter++;
		}
		else
			iter = mReloadables.erase(iter);	
	}
}

void ResourceManager::reloadAll()
{
	auto iter = mReloadables.cbegin();
	while(iter != mReloadables.cend())
	{
		std::shared_ptr<ReloadableInfo> info = *iter;

		if(!info->data.expired())
		{
			if (info->reload)
			{
				info->data.lock()->reload();
				info->reload = false;
			}

			iter++;
		}
		else
			iter = mReloadables.erase(iter);		
	}
}

void ResourceManager::addReloadable(std::weak_ptr<IReloadable> reloadable)
{
	std::shared_ptr<ReloadableInfo> info = std::make_shared<ReloadableInfo>();
	info->data = reloadable;
	info->reload = false;
	mReloadables.push_back(info);
}
