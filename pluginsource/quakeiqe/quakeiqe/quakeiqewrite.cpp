#include "stdafx.h"
#include "quakeiqe.h"

typedef struct iqeAnimHold_s
{
	RichMat43			*mats;
	int					numFrames;
	float				frameRate;
	int					numBones;
} iqeAnimHold_t;


//retrives animation data
static iqeAnimHold_t *Model_IQE_GetAnimData(noeRAPI_t *rapi)
{
	int animDataSize;
	BYTE *animData = rapi->Noesis_GetExtraAnimData(animDataSize);
	if (!animData)
	{
		return NULL;
	}

	noesisAnim_t *anim = rapi->Noesis_AnimAlloc("animout", animData, animDataSize); //animation containers are pool-allocated, so don't worry about freeing them
	//copy off the raw matrices for the animation frames
	iqeAnimHold_t *iqea = (iqeAnimHold_t *)rapi->Noesis_PooledAlloc(sizeof(iqeAnimHold_t));
	memset(iqea, 0, sizeof(iqeAnimHold_t));
	iqea->mats = (RichMat43 *)rapi->rpgMatsFromAnim(anim, iqea->numFrames, iqea->frameRate, &iqea->numBones, true);
	return iqea;
}

//export to iqe
//this just shoves everything out in a fairly fixed format. if people actually have need of it, i'll add more options.
bool Model_IQE_Write(noesisModel_t *mdl, RichBitStream *outStream, noeRAPI_t *rapi)
{
	//MessageBox(g_nfn->NPAPI_GetMainWnd(), L"You could spawn an export options dialog resource here if you wanted.", L"Potential Prompt", MB_OK);

	iqeAnimHold_t *iqea = Model_IQE_GetAnimData(rapi);
	sharedModel_t *pmdl = rapi->rpgGetSharedModel(mdl,
													NMSHAREDFL_WANTNEIGHBORS | //calculate triangle neighbors (can be timely on complex models, but this format wants them)
													NMSHAREDFL_WANTGLOBALARRAY | //calculate giant flat vertex/triangle arrays
													NMSHAREDFL_WANTTANGENTS4 | //make sure tangents are available
													NMSHAREDFL_FLATWEIGHTS | //create flat vertex weight arrays
													NMSHAREDFL_FLATWEIGHTS_FORCE4 | //force 4 weights per vert for the flat weight array data
													NMSHAREDFL_REVERSEWINDING //reverse the face winding (as per Quake) - most formats will not want you to do this!
													);
	if (pmdl->numBones <= 0)
	{
		rapi->LogOutput("ERROR: IQE output is only supported for skeletal models!\n");
		return false;
	}

	// header
	outStream->WriteStringVA("%s", "# Inter-Quake Export\n");

	// joints
	for (int i = 0; i < pmdl->numBones; i++)
	{
		modelBone_t *bone = pmdl->bones+i;

		RichMat43 prel;
		if (bone->eData.parent)
		{
			RichMat43 pinv(bone->eData.parent->mat);
			pinv.Inverse();
			RichMat43 &bmat = *(RichMat43 *)&bone->mat;
			prel = bmat*pinv;
		}
		else
		{
			prel = *(RichMat43 *)&bone->mat;
		}

		outStream->WriteStringVA("joint \"%s\" %i\n"
			"\tpm %f %f %f  %f %f %f  %f %f %f  %f %f %f\n", 
			bone->name, bone->eData.parent ? bone->eData.parent->index : -1,
			prel.m.o[0], prel.m.o[1], prel.m.o[2],
			prel.m.x1[0], prel.m.x1[1], prel.m.x1[2],
			prel.m.x2[0], prel.m.x2[1], prel.m.x2[2],
			prel.m.x3[0], prel.m.x3[1], prel.m.x3[2]
		);
	}

	outStream->WriteStringVA("%s", "\n");

	// meshes
	for (int i = 0; i < pmdl->numMeshes; i++)
	{
		sharedMesh_t *meshSrc = pmdl->meshes+i;

		if (!meshSrc->verts || !meshSrc->tris || !meshSrc->triNeighbors)
		{
			rapi->LogOutput("ERROR: Encountered a mesh with no geometry in IQM export!\n");
			return false;
		}
		if (!meshSrc->flatBoneIdx || !meshSrc->flatBoneWgt)
		{
			rapi->LogOutput("ERROR: Encountered an unweighted mesh in IQM export!\n");
			return false;
		}

		outStream->WriteStringVA ("mesh \"%s\"\n" 
			"\tmaterial \"%s\"\n",
			meshSrc->name, meshSrc->skinName
		);

		outStream->WriteStringVA("%s", "\n");

		modelVert_t	*mv = meshSrc->verts;
		modelTexCoord_t *muv = meshSrc->uvs;
		modelVert_t *mn = meshSrc->normals;
		int *idx = meshSrc->flatBoneIdx;
		float *wgt = meshSrc->flatBoneWgt; 
		for (int j = 0; j < meshSrc->numVerts; j++)
		{
			// vertex position, texture coordinates and normal
			outStream->WriteStringVA(
				"vp %f %f %f\n"
				"\tvt %f %f\n"
				"\tvn %f %f %f\n",
				mv[j].x, mv[j].y, mv[j].z,
				muv[j].u, muv[j].v,
				mn[j].x, mn[j].y, mn[j].z
			);

			// influences
			outStream->WriteStringVA("\tvb");

			for (int k = 0; k < 4; k++)
			{
				outStream->WriteStringVA(" %i %f", idx[j*4+k], wgt[j*4+k] );
			}

			outStream->WriteStringVA("\n");
		}

		// triangles
		outStream->WriteStringVA("\n");
		
		modelLongTri_t *mtris = pmdl->absTris + meshSrc->firstTri;
		for (int j = 0; j < meshSrc->numTris; j++)
		{
			outStream->WriteStringVA("fm %i %i %i\n",
				mtris[j].idx[0], mtris[j].idx[1], mtris[j].idx[2]
			);
		}
	}

	if (iqea && iqea->mats && iqea->numBones == pmdl->numBones && iqea->numFrames)
	{
		// animation
		outStream->WriteStringVA("\n");

		outStream->WriteStringVA("animation\n"
			"\tframerate 0\n" );

		// frames
		for (int i = 0; i < iqea->numFrames; i++)
		{
			outStream->WriteStringVA("\n");

			outStream->WriteStringVA("frame\n#%i\n", i);

			for (int j = 0; j < pmdl->numBones; j++)
			{
				RichMat43 *animMat = iqea->mats + i*pmdl->numBones + j;

				outStream->WriteStringVA("pm %f %f %f  %f %f %f  %f %f %f  %f %f %f\n", 
					animMat->m.o[0], animMat->m.o[1], animMat->m.o[2],
					animMat->m.x1[0], animMat->m.x1[1], animMat->m.x1[2],
					animMat->m.x2[0], animMat->m.x2[1], animMat->m.x2[2],
					animMat->m.x3[0], animMat->m.x3[1], animMat->m.x3[2]
				);
			}
		}
	}

	return true;
}

//catch anim writes
//(note that this function would normally write converted data to a file at anim->filename, 
// but for this format it instead saves the data to combine with the model output)
void Model_IQE_WriteAnim(noesisAnim_t *anim, noeRAPI_t *rapi)
{
	if (!rapi->Noesis_HasActiveGeometry() || rapi->Noesis_GetActiveType() != g_fmtHandle)
	{
		return;
	}
	rapi->Noesis_SetExtraAnimData(anim->data, anim->dataLen);
}
