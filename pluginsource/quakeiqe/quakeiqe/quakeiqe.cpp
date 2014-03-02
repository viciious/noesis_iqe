//General requirements and/or suggestions for Noesis plugins:
//
// -Use 1-byte struct alignment for all shared structures. (plugin MSVC project file defaults everything to 1-byte-aligned)
// -Always clean up after yourself. Your plugin stays in memory the entire time Noesis is loaded, so you don't want to crap up the process heaps.
// -Try to use reliable type-checking, to ensure your plugin doesn't conflict with other file types and create false-positive situations.
// -Really try not to write crash-prone logic in your data check function! This could lead to Noesis crashing from trivial things like the user browsing files.
// -When using the rpg begin/end interface, always make your Vertex call last, as it's the function which triggers a push of the vertex with its current attributes.
// -!!!! Check the web site and documentation for updated suggestions/info! !!!!

#include "stdafx.h"
#include "quakeiqe.h"

extern bool Model_IQE_Write(noesisModel_t *mdl, RichBitStream *outStream, noeRAPI_t *rapi);
extern void Model_IQE_WriteAnim(noesisAnim_t *anim, noeRAPI_t *rapi);

const char *g_pPluginName = "quakeiqe";
const char *g_pPluginDesc = "IQE format exporter, by Victor Luchits.";

int g_fmtHandle = -1;

//called by Noesis to init the plugin
bool NPAPI_InitLocal(void)
{
	g_fmtHandle = g_nfn->NPAPI_Register("Inter-Quake Export", ".iqe");
	if (g_fmtHandle < 0)
	{
		return false;
	}

	//set the data handlers for this format
	g_nfn->NPAPI_SetTypeHandler_WriteModel(g_fmtHandle, Model_IQE_Write);
	g_nfn->NPAPI_SetTypeHandler_WriteAnim(g_fmtHandle, Model_IQE_WriteAnim);

	return true;
}

//called by Noesis before the plugin is freed
void NPAPI_ShutdownLocal(void)
{
	//nothing to do in this plugin
}

BOOL APIENTRY DllMain( HMODULE hModule,
                       DWORD  ul_reason_for_call,
                       LPVOID lpReserved
					 )
{
    return TRUE;
}
