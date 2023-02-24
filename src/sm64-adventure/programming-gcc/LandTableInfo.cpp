#include "stdafx.h"
#include "LandTableInfo.h"
#include <fstream>
#include <iostream>
using std::default_delete;
using std::ifstream;
using std::ios;
using std::istream;
using std::list;
using std::shared_ptr;
using std::streamoff;
using std::string;
#ifdef _MSC_VER
using std::wstring;
#endif /* _MSC_VER */

LandTableInfo::LandTableInfo(const char* filename)
{
	ifstream str(filename, ios::binary);
	init(str);
	str.close();
}

LandTableInfo::LandTableInfo(const string& filename)
{
	ifstream str(filename, ios::binary);
	init(str);
	str.close();
}

#ifdef _MSC_VER
LandTableInfo::LandTableInfo(const wchar_t* filename)
{
	ifstream str(filename, ios::binary);
	init(str);
	str.close();
}

LandTableInfo::LandTableInfo(const wstring& filename)
{
	ifstream str(filename, ios::binary);
	init(str);
	str.close();
}
#endif /* _MSC_VER */

LandTableInfo::LandTableInfo(istream& stream) { init(stream); }

LandTable* LandTableInfo::getlandtable() const { return (LandTable*)landtable; }

_OBJ_LANDTABLE* LandTableInfo::getobjlandtable() const { return landtable; }

const string& LandTableInfo::getauthor() const { return author; }

const string& LandTableInfo::gettool() const { return tool; }

const string& LandTableInfo::getdescription() const { return description; }

const uint8_t* LandTableInfo::getmetadata(uint32_t identifier, uint32_t& size)
{
	auto elem = metadata.find(identifier);
	if (elem == metadata.end())
	{
		size = 0;
		return nullptr;
	}
	else
	{
		size = elem->second.size;
		return elem->second.data;
	}
}

static const string empty;
const string& LandTableInfo::getlabel(void* data)
{
	auto elem = labels1.find(data);
	if (elem == labels1.end())
		return empty;
	else
		return elem->second;
}

void* LandTableInfo::getdata(const string& label)
{
	auto elem = labels2.find(label);
	if (elem == labels2.end())
		return nullptr;
	else
		return elem->second;
}

const std::unordered_map<string, void*>* LandTableInfo::getlabels() const
{
	return &labels2;
}

static string getstring(istream& stream)
{
	auto start = stream.tellg();
	while (stream.get() != 0)
		;
	auto size = stream.tellg() - start;
	char* buf = new char[(unsigned int)size];
	stream.seekg(start);
	stream.read(buf, size);
	string result(buf);
	delete[] buf;
	return result;
}

template<typename T>
static inline void fixptr(T*& ptr, intptr_t base)
{
	if (ptr != nullptr)
		ptr = (T*)((uint8_t*)ptr + base);
}

void LandTableInfo::fixmodelpointers(NJS_MODEL_SADX* model, intptr_t base)
{
	fixptr(model->points, base);
	fixptr(model->normals, base);
	if (model->meshsets != nullptr)
	{
		fixptr(model->meshsets, base);
		if (reallocateddata.find(model->meshsets) != reallocateddata.end())
			model->meshsets = (NJS_MESHSET_SADX*)reallocateddata[model->meshsets];
		else
		{
			auto tmp = new NJS_MESHSET_SADX[model->nbMeshset];
			reallocateddata[model->meshsets] = tmp;
			for (int i = 0; i < model->nbMeshset; i++)
			{
				memcpy(&tmp[i], &((NJS_MESHSET*)model->meshsets)[i], sizeof(NJS_MESHSET));
				tmp[i].buffer = nullptr;
			}
			model->meshsets = tmp;
			allocatedmem.push_back(shared_ptr<NJS_MESHSET_SADX>(model->meshsets, default_delete<NJS_MESHSET_SADX[]>()));
			for (int i = 0; i < model->nbMeshset; i++)
			{
				fixptr(model->meshsets[i].meshes, base);
				fixptr(model->meshsets[i].attrs, base);
				fixptr(model->meshsets[i].normals, base);
				fixptr(model->meshsets[i].vertcolor, base);
				fixptr(model->meshsets[i].vertuv, base);
			}
		}
	}
	fixptr(model->mats, base);
}

void LandTableInfo::fixobjectpointers(NJS_OBJECT* object, intptr_t base)
{
	if (object->model != nullptr)
	{
		object->model = (uint8_t*)object->model + base;
		if (reallocateddata.find(object->model) != reallocateddata.end())
		{
			object->model = reallocateddata[object->model];
		}
		else
		{
			auto* tmp = new NJS_MODEL_SADX;
			reallocateddata[object->model] = tmp;
			memcpy(tmp, object->model, sizeof(NJS_MODEL));
			tmp->buffer = nullptr;
			object->basicdxmodel = tmp;
			allocatedmem.push_back(shared_ptr<NJS_MODEL_SADX>(object->basicdxmodel, default_delete<NJS_MODEL_SADX>()));
			fixmodelpointers(object->basicdxmodel, base);
		}
	}
	if (object->child != nullptr)
	{
		object->child = (NJS_OBJECT*)((uint8_t*)object->child + base);
		if (fixedpointers.find(object->child) == fixedpointers.end())
		{
			fixedpointers.insert(object->child);
			fixobjectpointers(object->child, base);
		}
	}
	if (object->sibling != nullptr)
	{
		object->sibling = (NJS_OBJECT*)((uint8_t*)object->sibling + base);
		if (fixedpointers.find(object->sibling) == fixedpointers.end())
		{
			fixedpointers.insert(object->sibling);
			fixobjectpointers(object->sibling, base);
		}
	}
}

inline void fixmkeypointers(NJS_MKEY_P* mkey, intptr_t base, int count)
{
	for (int i = 0; i < count; i++)
		fixptr(mkey[i].key, base);
}

template<typename T>
inline void fixmdatapointers(T* mdata, intptr_t base, int count, int vertoff, int normoff)
{
	for (int c = 0; c < count; c++)
	{
		for (int i = 0; i < (int)LengthOfArray(mdata->p); i++)
		{
			fixptr(mdata->p[i], base);
			if (i == vertoff || i == normoff)
				fixmkeypointers((NJS_MKEY_P*)mdata->p[i], base, mdata->nb[i]);
		}

		++mdata;
	}
}

void LandTableInfo::fixmotionpointers(NJS_MOTION* motion, intptr_t base, int count)
{
	if (motion->mdata != nullptr)
	{
		motion->mdata = (uint8_t*)motion->mdata + base;
		if (fixedpointers.find(motion->mdata) == fixedpointers.end())
		{
			fixedpointers.insert(motion->mdata);

			int vertoff = -1;
			int normoff = -1;

			if (motion->type & NJD_MTYPE_VERT_4)
			{
				vertoff = 0;
				if (motion->type & NJD_MTYPE_POS_0)
					++vertoff;
				if (motion->type & NJD_MTYPE_ANG_1)
					++vertoff;
				if (motion->type & NJD_MTYPE_QUAT_1)
					++vertoff;
				if (motion->type & NJD_MTYPE_SCL_2)
					++vertoff;
				if (motion->type & NJD_MTYPE_TARGET_3)
					++vertoff;
				if (motion->type & NJD_MTYPE_VEC_3)
					++vertoff;
			}
			if (motion->type & NJD_MTYPE_NORM_5)
			{
				normoff = 0;
				if (motion->type & NJD_MTYPE_POS_0)
					++normoff;
				if (motion->type & NJD_MTYPE_ANG_1)
					++normoff;
				if (motion->type & NJD_MTYPE_QUAT_1)
					++normoff;
				if (motion->type & NJD_MTYPE_SCL_2)
					++normoff;
				if (motion->type & NJD_MTYPE_TARGET_3)
					++normoff;
				if (motion->type & NJD_MTYPE_VEC_3)
					++normoff;
				if (motion->type & NJD_MTYPE_VERT_4)
					++normoff;
			}
			switch (motion->inp_fn & 0xF)
			{
			case 0:
				break;
			case 1:
				fixmdatapointers((NJS_MDATA1*)motion->mdata, base, count, vertoff, normoff);
				break;
			case 2:
				fixmdatapointers((NJS_MDATA2*)motion->mdata, base, count, vertoff, normoff);
				break;
			case 3:
				fixmdatapointers((NJS_MDATA3*)motion->mdata, base, count, vertoff, normoff);
				break;
			case 4:
				fixmdatapointers((NJS_MDATA4*)motion->mdata, base, count, vertoff, normoff);
				break;
			case 5:
				fixmdatapointers((NJS_MDATA5*)motion->mdata, base, count, vertoff, normoff);
				break;
			default:
				throw;
			}
		}
	}
}

void LandTableInfo::fixactionpointers(NJS_ACTION* action, intptr_t base)
{
	int count = 0;
	if (action->object != nullptr)
	{
		action->object = (NJS_OBJECT*)((uint8_t*)action->object + base);
		if (fixedpointers.find(action->object) == fixedpointers.end())
		{
			fixedpointers.insert(action->object);
			fixobjectpointers(action->object, base);
		}
		count = action->object->countanimated();
	}
	if (action->motion != nullptr)
	{
		action->motion = (NJS_MOTION*)((uint8_t*)action->motion + base);
		if (fixedpointers.find(action->motion) == fixedpointers.end())
		{
			fixedpointers.insert(action->motion);
			fixmotionpointers(action->motion, base, count);
		}
	}
}

void LandTableInfo::fixlandtablepointers(_OBJ_LANDTABLE* landtable, intptr_t base)
{
	if (landtable->pLandEntry != nullptr)
	{
		landtable->pLandEntry = (_OBJ_LANDENTRY*)((uint8_t*)landtable->pLandEntry + base);
		for (int i = 0; i < landtable->ssCount; i++)
			if (landtable->pLandEntry[i].pObject != nullptr)
			{
				landtable->pLandEntry[i].pObject = (NJS_OBJECT*)((uint8_t*)landtable->pLandEntry[i].pObject + base);
				if (fixedpointers.find(landtable->pLandEntry[i].pObject) == fixedpointers.end())
				{
					fixedpointers.insert(landtable->pLandEntry[i].pObject);
					fixobjectpointers(landtable->pLandEntry[i].pObject, base);
				}
			}
	}
	if (landtable->pMotLandEntry != nullptr)
	{
		landtable->pMotLandEntry = (_OBJ_MOTLANDENTRY*)((uint8_t*)landtable->pMotLandEntry + base);
		for (int i = 0; i < landtable->ssMotCount; i++)
		{
			if (landtable->pMotLandEntry[i].pObject != nullptr)
			{
				landtable->pMotLandEntry[i].pObject = (NJS_OBJECT*)((uint8_t*)landtable->pMotLandEntry[i].pObject + base);
				if (fixedpointers.find(landtable->pMotLandEntry[i].pObject) == fixedpointers.end())
				{
					fixedpointers.insert(landtable->pMotLandEntry[i].pObject);
					fixobjectpointers(landtable->pMotLandEntry[i].pObject, base);
				}
			}
			if (landtable->pMotLandEntry[i].pMotion != nullptr)
			{
				landtable->pMotLandEntry[i].pMotion = (NJS_ACTION*)((uint8_t*)landtable->pMotLandEntry[i].pMotion + base);
				if (fixedpointers.find(landtable->pMotLandEntry[i].pMotion) == fixedpointers.end())
				{
					fixedpointers.insert(landtable->pMotLandEntry[i].pMotion);
					fixactionpointers(landtable->pMotLandEntry[i].pMotion, base);
				}
			}
		}
	}
	fixptr(landtable->pPvmFileName, base);
}

template<typename T>
static inline void readdata(istream& stream, T& data)
{
	stream.read((char*)&data, sizeof(T));
}

void LandTableInfo::init(istream& stream)
{
	uint64_t magic;
	readdata(stream, magic);
	uint8_t version = magic >> 56;
	magic &= FormatMask;
	if (version != CurrentVersion) // unrecognized file version
		return;
	if (magic != SA1LVL)
		return;
	uint32_t landtableoff;
	readdata(stream, landtableoff);
	landtableoff -= headerSize;
	uint32_t tmpaddr;
	readdata(stream, tmpaddr);
	size_t mdlsize = tmpaddr - headerSize;
	auto* landtablebuf = new uint8_t[mdlsize];
	allocatedmem.push_back(shared_ptr<uint8_t>(landtablebuf, default_delete<uint8_t[]>()));
	stream.read((char*)landtablebuf, mdlsize);
	landtable = (_OBJ_LANDTABLE*)(landtablebuf + landtableoff);
	intptr_t landtablebase = (intptr_t)landtablebuf - headerSize;
	fixlandtablepointers(landtable, landtablebase);
	fixedpointers.clear();
	uint32_t chunktype;
	readdata(stream, chunktype);
	while (chunktype != ChunkTypes_End)
	{
		uint32_t chunksz;
		readdata(stream, chunksz);
		auto chunkbase = stream.tellg();
		auto nextchunk = chunkbase + (streamoff)chunksz;
		switch (chunktype)
		{
			case ChunkTypes_Label:
				while (true)
				{
					void* dataptr;
					readdata(stream, dataptr);
					uint32_t labelptr;
					readdata(stream, labelptr);

					if (dataptr == (void*)-1 && labelptr == UINT32_MAX)
					{
						break;
					}

					dataptr = (uint8_t*)dataptr + landtablebase;

					if (reallocateddata.find(dataptr) != reallocateddata.end())
					{
						dataptr = reallocateddata[dataptr];
					}

					tmpaddr = (uint32_t)stream.tellg();
					stream.seekg((uint32_t)chunkbase + labelptr);
					string label = getstring(stream);
					stream.seekg(tmpaddr);
					labels1[dataptr] = label;
					labels2[label] = dataptr;
				}
				break;
			case ChunkTypes_Author:
				author = getstring(stream);
				break;
			case ChunkTypes_Tool:
				tool = getstring(stream);
				break;
			case ChunkTypes_Description:
				description = getstring(stream);
				break;
			default:
				auto* buf = new uint8_t[chunksz];
				allocatedmem.push_back(shared_ptr<uint8_t>(buf, default_delete<uint8_t[]>()));
				stream.read((char*)buf, chunksz);
				Metadata meta = { chunksz, buf };
				metadata[chunktype] = meta;
				break;
		}
		stream.seekg(nextchunk);
		readdata(stream, chunktype);
	}
	reallocateddata.clear();
}
