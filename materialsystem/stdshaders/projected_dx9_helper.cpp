//===== Copyright � 1996-2005, Valve Corporation, All rights reserved. ======//
//
// Purpose: 
//
// $NoKeywords: $
//===========================================================================//

#include "BaseVSShader.h"
#include "projected_dx9_helper.h"
#include "..\shaderapidx9\locald3dtypes.h"												   
#include "convar.h"
#include "cpp_shader_constant_register_map.h"
#include "projected_vs20.inc"
#include "projected_vs30.inc"

#include "projected_ps20.inc"
#include "projected_ps20b.inc"
#include "projected_ps30.inc"

// NOTE: This has to be the last file included!
#include "tier0/memdbgon.h"


void InitParamsProjected_DX9( CBaseVSShader *pShader, IMaterialVar** params, const char *pMaterialName, Projected_DX9_Vars_t &info )
{
	SET_FLAGS( MATERIAL_VAR_NOCULL );
	SET_FLAGS2( MATERIAL_VAR2_SUPPORTS_HW_SKINNING );
}

void InitProjected_DX9( CBaseVSShader *pShader, IMaterialVar** params, Projected_DX9_Vars_t &info )
{
	if ( params[ info.m_nBaseTexture ]->IsDefined() )
	{
		pShader->LoadTexture( info.m_nBaseTexture );
	}

	if ( info.m_nFoW != -1 && params[ info.m_nFoW ]->IsDefined() )
	{
		pShader->LoadTexture( info.m_nFoW );
	}
}

void DrawProjected_DX9( CBaseVSShader *pShader, IMaterialVar** params, IShaderDynamicAPI *pShaderAPI,
				    IShaderShadow* pShaderShadow, Projected_DX9_Vars_t &info, VertexCompressionType_t vertexCompression )
{
	bool bIsModel = IS_FLAG_SET( MATERIAL_VAR_MODEL );
	bool bHasFoW = ( ( info.m_nFoW != -1 ) && ( params[ info.m_nFoW ]->IsTexture() != 0 ) );
	if ( bHasFoW == true )
	{
		ITexture *pTexture = params[ info.m_nFoW ]->GetTextureValue();
		if ( ( pTexture->GetFlags() & TEXTUREFLAGS_RENDERTARGET ) == 0 )
		{
			bHasFoW = false;
		}
	}

	SHADOW_STATE
	{
		pShader->SetInitialShadowState( );

		pShaderShadow->EnableSRGBRead( SHADER_SAMPLER1, true );	// Always SRGB read on base map 1

		pShaderShadow->EnableTexture( SHADER_SAMPLER1, true );
		if ( bHasFoW )
		{
//			pShaderShadow->EnableSRGBRead( SHADER_SAMPLER10, true );	// Always SRGB read on base map
			pShaderShadow->EnableTexture( SHADER_SAMPLER10, true );
		}

		pShaderShadow->EnableSRGBWrite( true );

		unsigned int flags = VERTEX_POSITION | VERTEX_FORMAT_COMPRESSED;
		int nTexCoordCount = 0;

		pShaderShadow->VertexShaderVertexFormat( flags, nTexCoordCount, NULL, 0 );

#ifndef _X360
		if ( !g_pHardwareConfig->HasFastVertexTextures() )
#endif
		{
			DECLARE_STATIC_VERTEX_SHADER( projected_vs20 );
			SET_STATIC_VERTEX_SHADER_COMBO( FOW, bHasFoW );
			SET_STATIC_VERTEX_SHADER_COMBO( MODEL,  bIsModel );
			SET_STATIC_VERTEX_SHADER( projected_vs20 );

			// Bind ps_2_b shader so we can get Phong terms
			if ( g_pHardwareConfig->SupportsPixelShaders_2_b() )
			{
				DECLARE_STATIC_PIXEL_SHADER( projected_ps20b );
				SET_STATIC_PIXEL_SHADER_COMBO( FOW, bHasFoW );
				SET_STATIC_PIXEL_SHADER( projected_ps20b );
			}
			else
			{
				DECLARE_STATIC_PIXEL_SHADER( projected_ps20 );
				SET_STATIC_PIXEL_SHADER_COMBO( FOW, bHasFoW );
				SET_STATIC_PIXEL_SHADER( projected_ps20 );
			}
		}
#ifndef _X360
		else
		{
			// The vertex shader uses the vertex id stream
			SET_FLAGS2( MATERIAL_VAR2_USES_VERTEXID );

			DECLARE_STATIC_VERTEX_SHADER( projected_vs30 );
			SET_STATIC_VERTEX_SHADER_COMBO( MODEL,  bIsModel );
			SET_STATIC_VERTEX_SHADER_COMBO( FOW, bHasFoW );
			SET_STATIC_VERTEX_SHADER( projected_vs30 );

			// Bind ps_2_b shader so we can get Phong terms
			DECLARE_STATIC_PIXEL_SHADER( projected_ps30 );
			SET_STATIC_PIXEL_SHADER_COMBO( FOW, bHasFoW );
			SET_STATIC_PIXEL_SHADER( projected_ps30 );
		}
#endif

		pShaderShadow->EnablePolyOffset( SHADER_POLYOFFSET_DECAL );
		pShaderShadow->EnableDepthTest( true );
		pShaderShadow->EnableDepthWrites( false );

		pShader->DefaultFog();

		pShader->SetAdditiveBlendingShadowState( info.m_nBaseTexture, true );
		pShaderShadow->EnableBlending( true );

		// Lighting constants
		pShader->PI_BeginCommandBuffer();
		pShader->PI_SetPixelShaderAmbientLightCube( PSREG_AMBIENT_CUBE );
		pShader->PI_SetPixelShaderLocalLighting( PSREG_LIGHT_INFO_ARRAY );
		pShader->PI_EndCommandBuffer();
	}
	DYNAMIC_STATE
	{
		pShaderAPI->SetDefaultState();

		int numBones = pShaderAPI->GetCurrentNumBones();

		// Bind textures
		pShader->BindTexture( SHADER_SAMPLER1, info.m_nBaseTexture );							// Base Map 1

		if ( bHasFoW )
		{
			pShader->BindTexture( SHADER_SAMPLER10, info.m_nFoW, -1 );

			float	vFoWSize[ 4 ];
			Vector	vMins = pShaderAPI->GetVectorRenderingParameter( VECTOR_RENDERPARM_GLOBAL_FOW_MINS );
			Vector	vMaxs = pShaderAPI->GetVectorRenderingParameter( VECTOR_RENDERPARM_GLOBAL_FOW_MAXS );
			vFoWSize[ 0 ] = vMins.x;
			vFoWSize[ 1 ] = vMins.y;
			vFoWSize[ 2 ] = vMaxs.x - vMins.x;
			vFoWSize[ 3 ] = vMaxs.y - vMins.y;
			pShaderAPI->SetVertexShaderConstant( 26, vFoWSize );
		}

#ifndef _X360
		if ( !g_pHardwareConfig->HasFastVertexTextures() )
#endif
		{
			DECLARE_DYNAMIC_VERTEX_SHADER( projected_vs20 );
			SET_DYNAMIC_VERTEX_SHADER_COMBO( SKINNING, ( numBones > 0 ) );
			SET_DYNAMIC_VERTEX_SHADER_COMBO( COMPRESSED_VERTS, ( int )vertexCompression );
			SET_DYNAMIC_VERTEX_SHADER( projected_vs20 );

			// Bind ps_2_b shader so we can get Phong, rim and a cloudier refraction
			if ( g_pHardwareConfig->SupportsPixelShaders_2_b() )
			{
				DECLARE_DYNAMIC_PIXEL_SHADER( projected_ps20b );
				SET_DYNAMIC_PIXEL_SHADER( projected_ps20b );
			}
			else
			{
				DECLARE_DYNAMIC_PIXEL_SHADER( projected_ps20 );
				SET_DYNAMIC_PIXEL_SHADER( projected_ps20 );
			}
		}
#ifndef _X360
		else
		{
			DECLARE_DYNAMIC_VERTEX_SHADER( projected_vs30 );
			SET_DYNAMIC_VERTEX_SHADER_COMBO( SKINNING, ( numBones > 0 ) );
			SET_DYNAMIC_VERTEX_SHADER_COMBO( COMPRESSED_VERTS, ( int )vertexCompression );
			SET_DYNAMIC_VERTEX_SHADER( projected_vs30 );

			DECLARE_DYNAMIC_PIXEL_SHADER( projected_ps30 );
			SET_DYNAMIC_PIXEL_SHADER( projected_ps30 );
		}
#endif

		pShader->SetVertexShaderTextureTransform( VERTEX_SHADER_SHADER_SPECIFIC_CONST_6, info.m_nBaseTextureTransform );

		pShaderAPI->SetPixelShaderFogParams( PSREG_FOG_PARAMS );

		VMatrix worldToTexture;
		FlashlightState_t state = pShaderAPI->GetFlashlightState( worldToTexture );

		Vector4D vLightDir;
		vLightDir.AsVector3D() = state.m_vecLightOrigin;
		pShaderAPI->SetVertexShaderConstant( VERTEX_SHADER_SHADER_SPECIFIC_CONST_0, vLightDir.Base() );

		Vector4D vProjectionSize( state.m_flProjectionSize * 2.0f, state.m_flProjectionRotation, 0.0f, 0.0f );
		pShaderAPI->SetVertexShaderConstant( VERTEX_SHADER_SHADER_SPECIFIC_CONST_1, vProjectionSize.Base() );

		pShaderAPI->SetPixelShaderConstant( PSREG_FLASHLIGHT_COLOR, &state.m_Color[ 0 ], 1 );

		// Set c0 and c1 to contain first two rows of ViewProj matrix
		VMatrix matView, matProj, matViewProj;
		pShaderAPI->GetMatrix( MATERIAL_VIEW, matView.m[0] );
		pShaderAPI->GetMatrix( MATERIAL_PROJECTION, matProj.m[0] );
		matViewProj = matView * matProj;
		pShaderAPI->SetPixelShaderConstant( 0, matViewProj.m[0], 2 );
	}
	pShader->Draw();
}
