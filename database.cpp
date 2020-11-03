// Copyright (c) 2015-2020 Vector 35 Inc
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to
// deal in the Software without restriction, including without limitation the
// rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
// sell copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
// FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
// IN THE SOFTWARE.
#include <cstring>
#include "binaryninjaapi.h"

using namespace BinaryNinja;
using namespace Json;
using namespace std;


struct ProgressContext
{
	std::function<void(size_t, size_t)> callback;
};


void ProgressCallback(void* ctxt, size_t current, size_t total)
{
	ProgressContext* pctxt = reinterpret_cast<ProgressContext*>(ctxt);
	pctxt->callback(current, total);
}


KeyValueStore::KeyValueStore()
{
	m_object = BNCreateKeyValueStore();
}


KeyValueStore::KeyValueStore(BNKeyValueStore* store)
{
	m_object = store;
}


std::vector<std::string> KeyValueStore::GetKeys() const
{
	size_t count;
	char** keys = BNGetKeyValueStoreKeys(m_object, &count);

	std::vector<std::string> strings;
	strings.reserve(count);
	for (size_t i = 0; i < count; ++i)
	{
		strings.push_back(keys[i]);
	}

	BNFreeStringList(keys, count);
	return strings;
}


bool KeyValueStore::HasValue(const std::string& name) const
{
	return BNKeyValueStoreHasValue(m_object, name.c_str());
}


Json::Value KeyValueStore::GetValue(const std::string& name) const
{
	DataBuffer value = DataBuffer(BNGetKeyValueStoreBuffer(m_object, name.c_str()));
	Json::Value json;
	std::unique_ptr<Json::CharReader> reader(Json::CharReaderBuilder().newCharReader());
	std::string errors;
	if (!reader->parse(static_cast<const char*>(value.GetData()),
	                   static_cast<const char*>(value.GetDataAt(value.GetLength())),
	                   &json, &errors))
	{
		throw Exception(errors);
	}
	return json;
}


DataBuffer KeyValueStore::GetBuffer(const std::string& name) const
{
	BNDataBuffer* buffer = BNGetKeyValueStoreBuffer(m_object, name.c_str());
	if (buffer == nullptr)
	{
		throw Exception("Unknown key");
	}
	return DataBuffer(buffer);
}


void KeyValueStore::SetValue(const std::string& name, const Json::Value& value)
{
	Json::StreamWriterBuilder builder;
	builder["indentation"] = "";
	string json = Json::writeString(builder, value);
	if (!BNSetKeyValueStoreValue(m_object, name.c_str(), json.c_str()))
	{
		throw Exception("BNSetKeyValueStoreValue");
	}
}


void KeyValueStore::SetBuffer(const std::string& name, const DataBuffer& value)
{
	if (!BNSetKeyValueStoreBuffer(m_object, name.c_str(), value.GetBufferObject()))
	{
		throw Exception("BNSetKeyValueStoreBuffer");
	}
}


DataBuffer KeyValueStore::GetSerializedData() const
{
	return DataBuffer(BNGetKeyValueStoreSerializedData(m_object));
}


void KeyValueStore::BeginNamespace(const std::string& name)
{
	BNBeginKeyValueStoreNamespace(m_object, name.c_str());
}


void KeyValueStore::EndNamespace()
{
	BNEndKeyValueStoreNamespace(m_object);
}


bool KeyValueStore::IsEmpty() const
{
	return BNIsKeyValueStoreEmpty(m_object);
}


size_t KeyValueStore::ValueSize() const
{
	return BNGetKeyValueStoreValueSize(m_object);
}


size_t KeyValueStore::DataSize() const
{
	return BNGetKeyValueStoreDataSize(m_object);
}


size_t KeyValueStore::ValueStorageSize() const
{
	return BNGetKeyValueStoreValueStorageSize(m_object);
}


size_t KeyValueStore::NamespaceSize() const
{
	return BNGetKeyValueStoreNamespaceSize(m_object);
}


Snapshot::Snapshot(BNSnapshot* snapshot)
{
	m_object = snapshot;
}


int64_t Snapshot::GetId()
{
	return BNGetSnapshotId(m_object);
}


std::string Snapshot::GetName()
{
	char* cstr = BNGetSnapshotName(m_object);
	std::string str{cstr};
	BNFreeString(cstr);
	return str;
}


bool Snapshot::IsAutoSave()
{
	return BNIsSnapshotAutoSave(m_object);
}


Ref<Snapshot> Snapshot::GetParent()
{
	BNSnapshot* snap = BNGetSnapshotParent(m_object);
	if (snap == nullptr)
		return nullptr;
	return new Snapshot(snap);
}


DataBuffer Snapshot::GetFileContents()
{
	BNDataBuffer* buffer = BNGetSnapshotFileContents(m_object);
	if (buffer == nullptr)
	{
		throw Exception("BNGetSnapshotFileContents");
	}
	return DataBuffer(buffer);
}


vector<UndoEntry> Snapshot::GetUndoEntries()
{
	return GetUndoEntries([](size_t, size_t){});
}


vector<UndoEntry> Snapshot::GetUndoEntries(const std::function<void(size_t, size_t)>& progress)
{
	ProgressContext pctxt;
	pctxt.callback = progress;

	size_t numEntries;
	BNUndoEntry* entries = BNGetSnapshotUndoEntriesWithProgress(m_object, &pctxt, ProgressCallback, &numEntries);
	if (entries == nullptr)
	{
		throw Exception("BNGetSnapshotUndoEntries");
	}

	vector<UndoEntry> result;
	result.reserve(numEntries);
	for (size_t i = 0; i < numEntries; i++)
	{
		UndoEntry temp;
		temp.timestamp = entries[i].timestamp;
		temp.hash = entries[i].hash;
		temp.user = new User(BNNewUserReference(entries[i].user));
		size_t actionCount = entries[i].actionCount;
		for (size_t actionIndex = 0; actionIndex < actionCount; actionIndex++)
		{
			temp.actions.emplace_back(entries[i].actions[actionIndex]);
		}
		result.push_back(temp);
	}

	BNFreeUndoEntries(entries, numEntries);
	return result;
}


Ref<KeyValueStore> Snapshot::ReadData()
{
	return ReadData([](size_t, size_t){});
}


Ref<KeyValueStore> Snapshot::ReadData(const std::function<void(size_t, size_t)>& progress)
{
	ProgressContext pctxt;
	pctxt.callback = progress;
	BNKeyValueStore* store = BNReadSnapshotDataWithProgress(m_object, &pctxt, ProgressCallback);
	if (store == nullptr)
	{
		throw Exception("BNReadSnapshotData");
	}
	return new KeyValueStore(store);
}


Database::Database(BNDatabase* database)
{
	m_object = database;
}


Ref<Snapshot> Database::GetSnapshot(int64_t id)
{
	BNSnapshot* snap = BNGetDatabaseSnapshot(m_object, id);
	if (snap == nullptr)
		return nullptr;
	return new Snapshot(snap);
}


Ref<Snapshot> Database::GetCurrentSnapshot()
{
	BNSnapshot* snap = BNGetDatabaseCurrentSnapshot(m_object);
	if (snap == nullptr)
		return nullptr;
	return new Snapshot(snap);
}


int64_t Database::WriteSnapshotData(int64_t parent, Ref<BinaryView> file, const std::string& name, const Ref<KeyValueStore>& data, bool autoSave, const std::function<void(size_t, size_t)>& progress)
{
	ProgressContext pctxt;
	pctxt.callback = progress;
	int64_t result = BNWriteDatabaseSnapshotData(m_object, parent, file->GetObject(), name.c_str(), data->GetObject(), autoSave, &pctxt, ProgressCallback);
	if (result < 0)
	{
		throw Exception("BNWriteDatabaseSnapshotData");
	}
	return result;
}


Json::Value Database::ReadGlobal(const std::string& key) const
{
	char* value = BNReadDatabaseGlobal(m_object, key.c_str());
	if (value == nullptr)
	{
		throw Exception("BNReadDatabaseGlobal");
	}

	Json::Value json;
	std::unique_ptr<Json::CharReader> reader(Json::CharReaderBuilder().newCharReader());
	std::string errors;
	if (!reader->parse(value, value + strlen(value), &json, &errors))
	{
		throw Exception(errors);
	}

	BNFreeString(value);
	return json;
}


void Database::WriteGlobal(const std::string& key, const Json::Value& val)
{
	Json::StreamWriterBuilder builder;
	builder["indentation"] = "";
	string json = Json::writeString(builder, val);
	if (!BNWriteDatabaseGlobal(m_object, key.c_str(), json.c_str()))
	{
		throw Exception("BNWriteDatabaseGlobal");
	}
}


DataBuffer Database::ReadGlobalData(const std::string& key) const
{
	BNDataBuffer* value = BNReadDatabaseGlobalData(m_object, key.c_str());
	if (value == nullptr)
	{
		throw Exception("BNReadDatabaseGlobalData");
	}
	return DataBuffer(value);
}


void Database::WriteGlobalData(const std::string& key, const DataBuffer& val)
{
	if (!BNWriteDatabaseGlobalData(m_object, key.c_str(), val.GetBufferObject()))
	{
		throw Exception("BNWriteDatabaseGlobalData");
	}
}


Ref<FileMetadata> Database::GetFile()
{
	return new FileMetadata(BNGetDatabaseFile(m_object));
}
