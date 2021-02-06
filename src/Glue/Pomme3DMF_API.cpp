#include <cstring>
#include "Pomme.h"
#include "PommeFiles.h"
#include "Pomme3DMF.h"
#include "Pomme3DMF_Internal.h"

#if __APPLE__
	#include <OpenGL/glu.h>
#else
	#include <GL/glu.h>
#endif

#define ALLOCATOR_HEADER_BYTES 16

// TODO: use __Q3Alloc for C++ classes as well
#define CHECK_COOKIE(obj)												\
	do																	\
	{																	\
		if ( (obj).cookie != (obj).COOKIE )								\
			throw std::runtime_error("Pomme3DMF: illegal cookie");		\
	} while (0)

static void Assert(bool condition, const char* message)
{
	if (!condition)
	{
		throw std::runtime_error(message);
	}
}

static void ThrowGLError(GLenum error, const char* func, int line)
{
	char message[512];

	snprintf(message, sizeof(message), "OpenGL error 0x%x in %s:%d (\"%s\")",
				error, func, line, (const char*) gluErrorString(error));

	throw std::runtime_error(message);
}

#define CHECK_GL_ERROR()												\
	do {					 											\
		GLenum error = glGetError();									\
		if (error != GL_NO_ERROR)										\
			ThrowGLError(error, __func__, __LINE__);					\
	} while(0)

struct __Q3AllocatorCookie
{
	uint32_t		classID;
	uint32_t		blockSize;		// including header cookie
};

static_assert(sizeof(__Q3AllocatorCookie) <= ALLOCATOR_HEADER_BYTES);

template<typename T>
static T* __Q3Alloc(size_t count, uint32_t classID)
{
	size_t totalBytes = ALLOCATOR_HEADER_BYTES + count*sizeof(T);

	uint8_t* block = (uint8_t*) calloc(totalBytes, 1);

	__Q3AllocatorCookie* cookie = (__Q3AllocatorCookie*) block;

	cookie->classID		= classID;
	cookie->blockSize	= totalBytes;

	return (T*) (block + ALLOCATOR_HEADER_BYTES);
}

static __Q3AllocatorCookie* __Q3GetCookie(const void* sourcePayload, uint32_t classID)
{
	if (!sourcePayload)
		throw std::runtime_error("__Q3GetCookie: got null pointer");

	uint8_t* block = ((uint8_t*) sourcePayload) - ALLOCATOR_HEADER_BYTES;

	__Q3AllocatorCookie* cookie = (__Q3AllocatorCookie*) block;

	if (classID != cookie->classID)
		throw std::runtime_error("__Q3GetCookie: incorrect cookie");

	return cookie;
}

template<typename T>
static T* __Q3Copy(const T* sourcePayload, uint32_t classID)
{
	if (!sourcePayload)
		return nullptr;

	__Q3AllocatorCookie* sourceCookie = __Q3GetCookie(sourcePayload, classID);

	uint8_t* block = (uint8_t*) calloc(sourceCookie->blockSize, 1);
	memcpy(block, sourceCookie, sourceCookie->blockSize);

	return (T*) (block + ALLOCATOR_HEADER_BYTES);
}

static void __Q3Dispose(void* object, uint32_t classID)
{
	if (!object)
		return;

	auto cookie = __Q3GetCookie(object, classID);
	cookie->classID = 'DEAD';

	free(cookie);
}

template<typename T>
static void __Q3DisposeArray(T** arrayPtr, uint32_t cookie)
{
	if (*arrayPtr)
	{
		__Q3Dispose(*arrayPtr, cookie);
		*arrayPtr = nullptr;
	}
}

Pomme3DMF_FileHandle Pomme3DMF_LoadModelFile(const FSSpec* spec)
{
	short refNum;
	OSErr err;

	err = FSpOpenDF(spec, fsRdPerm, &refNum);
	if (err != noErr)
		return nullptr;

	printf("========== LOADING 3DMF: %s ===========\n", spec->cName);

	Q3MetaFile* metaFile = new Q3MetaFile();

	auto& fileStream = Pomme::Files::GetStream(refNum);
	Q3MetaFileParser(fileStream, *metaFile).Parse3DMF();
	FSClose(refNum);

	//-------------------------------------------------------------------------
	// Load textures

	for (uint32_t i = 0; i < metaFile->textures.size(); i++)
	{
		auto& textureDef = metaFile->textures[i];
		Assert(textureDef.glTextureName == 0, "texture already allocated");

		GLuint textureName;

		glGenTextures(1, &textureName);
		CHECK_GL_ERROR();

		printf("Loading GL texture #%d\n", textureName);

		textureDef.glTextureName = textureName;

		glBindTexture(GL_TEXTURE_2D, textureName);				// this is now the currently active texture
		CHECK_GL_ERROR();

		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

//			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
//			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

		GLenum internalFormat;
		GLenum format;
		GLenum type;
		switch (textureDef.pixelType)
		{
			case kQ3PixelTypeRGB16:
				internalFormat = GL_RGB;
				format = GL_BGRA_EXT;
				type = GL_UNSIGNED_SHORT_1_5_5_5_REV;
				break;
			case kQ3PixelTypeARGB16:
				internalFormat = GL_RGBA;
				format = GL_BGRA_EXT;
				type = GL_UNSIGNED_SHORT_1_5_5_5_REV;
				break;
			default:
				throw std::runtime_error("3DMF texture: Unsupported kQ3PixelType");
		}

		glTexImage2D(GL_TEXTURE_2D,
					 0,										// mipmap level
					 internalFormat,						// format in OpenGL
					 textureDef.width,						// width in pixels
					 textureDef.height,						// height in pixels
					 0,										// border
					 format,								// what my format is
					 type,									// size of each r,g,b
					 textureDef.buffer.data());				// pointer to the actual texture pixels
		CHECK_GL_ERROR();

		// Set glTextureName on meshes
		for (auto mesh : metaFile->meshes)
		{
			if (mesh->hasTexture && mesh->internalTextureID == i)
			{
				mesh->glTextureName = textureName;
			}
		}
	}

	//-------------------------------------------------------------------------
	// Done

	return (Pomme3DMF_FileHandle) metaFile;
}

void Pomme3DMF_DisposeModelFile(Pomme3DMF_FileHandle the3DMFFile)
{
	if (!the3DMFFile)
		return;

	auto metaFile = (Q3MetaFile*) the3DMFFile;
	CHECK_COOKIE(*metaFile);
	delete metaFile;
}

TQ3TriMeshFlatGroup Pomme3DMF_GetAllMeshes(Pomme3DMF_FileHandle the3DMFFile)
{
	auto& metaFile = *(Q3MetaFile*) the3DMFFile;
	CHECK_COOKIE(metaFile);

	TQ3TriMeshFlatGroup list;
	list.numMeshes		= metaFile.meshes.size();
	list.meshes			= metaFile.meshes.data();
	return list;
}

int Pomme3DMF_CountTopLevelMeshGroups(Pomme3DMF_FileHandle the3DMFFile)
{
	auto& metaFile = *(Q3MetaFile*) the3DMFFile;
	CHECK_COOKIE(metaFile);

	return metaFile.topLevelMeshGroups.size();
}

TQ3TriMeshFlatGroup Pomme3DMF_GetTopLevelMeshGroup(Pomme3DMF_FileHandle the3DMFFile, int groupNumber)
{
	auto& metaFile = *(Q3MetaFile*) the3DMFFile;
	CHECK_COOKIE(metaFile);

	auto& internalGroup = metaFile.topLevelMeshGroups.at(groupNumber);

	TQ3TriMeshFlatGroup group;
	group.numMeshes = internalGroup.size();
	group.meshes = internalGroup.data();
	return group;
}

TQ3TriMeshData* Q3TriMeshData_New(int numTriangles,	int numPoints)
{
	TQ3TriMeshData* mesh	= __Q3Alloc<TQ3TriMeshData>(1, 'TMSH');

	mesh->numTriangles		= numTriangles;
	mesh->numPoints			= numPoints;
	mesh->points			= __Q3Alloc<TQ3Point3D>(numPoints, 'TMpt');
	mesh->triangles			= __Q3Alloc<TQ3TriMeshTriangleData>(numTriangles, 'TMtr');
	mesh->vertexNormals		= __Q3Alloc<TQ3Vector3D>(numPoints, 'TMvn');
	mesh->vertexUVs			= __Q3Alloc<TQ3Param2D>(numPoints, 'TMuv');
	mesh->vertexColors		= nullptr;
	mesh->diffuseColor		= {1, 1, 1, 1};
	mesh->hasTexture		= false;
	mesh->textureHasTransparency = false;

	for (int i = 0; i < numPoints; i++)
	{
		mesh->vertexNormals[i] = {0, 1, 0};
		mesh->vertexUVs[i] = {.5f, .5f};
//		triMeshData->vertexColors[i] = {1, 1, 1, 1};
	}

	return mesh;
}

TQ3TriMeshData* Q3TriMeshData_Duplicate(const TQ3TriMeshData* source)
{
	TQ3TriMeshData* mesh	= __Q3Copy(source, 'TMSH');
	mesh->points			= __Q3Copy(source->points,			'TMpt');
	mesh->triangles			= __Q3Copy(source->triangles,		'TMtr');
	mesh->vertexNormals		= __Q3Copy(source->vertexNormals,	'TMvn');
	mesh->vertexColors		= __Q3Copy(source->vertexColors,	'TMvc');
	mesh->vertexUVs			= __Q3Copy(source->vertexUVs,		'TMuv');
	return mesh;
}

void Q3TriMeshData_Dispose(TQ3TriMeshData* mesh)
{
	__Q3DisposeArray(&mesh->points,				'TMpt');
	__Q3DisposeArray(&mesh->triangles,			'TMtr');
	__Q3DisposeArray(&mesh->vertexNormals,		'TMvn');
	__Q3DisposeArray(&mesh->vertexColors,		'TMvc');
	__Q3DisposeArray(&mesh->vertexUVs,			'TMuv');
	__Q3Dispose(mesh, 'TMSH');
}
