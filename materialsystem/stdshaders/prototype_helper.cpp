//========= Copyright � 1996-2006, Valve Corporation, All rights reserved. ============//

#include "BaseVSShader.h"
#include "mathlib/VMatrix.h"
#include "prototype_helper.h"
#include "convar.h"

// Auto generated inc files
#include "prototype_vs20.inc"
#include "prototype_ps20b.inc"

void InitParamsPrototype( CBaseVSShader *pShader, IMaterialVar** params, const char *pMaterialName, PrototypeVars_t &info )
{
	// Set material flags
	SET_FLAGS2( MATERIAL_VAR2_SUPPORTS_HW_SKINNING );
	SET_FLAGS2( MATERIAL_VAR2_NEEDS_TANGENT_SPACES );
	SET_FLAGS2( MATERIAL_VAR2_LIGHTING_VERTEX_LIT );

	// Set material parameter default values
	if ( (info.m_nBaseTextureFrame != -1 ) && !params[info.m_nBaseTextureFrame]->IsDefined() )
	{
		params[info.m_nBaseTextureFrame]->SetIntValue( 0 );
	}

	if ( (info.m_nBumpFrame != -1 ) && !params[info.m_nBumpFrame]->IsDefined() )
	{
		params[info.m_nBumpFrame]->SetIntValue( 0 );
	}
}

void InitPrototype( CBaseVSShader *pShader, IMaterialVar** params, PrototypeVars_t &info )
{
	// Load textures
	if ( (info.m_nBaseTexture != -1) && params[info.m_nBaseTexture]->IsDefined() )
	{
		pShader->LoadTexture( info.m_nBaseTexture );
	}

	if ( (info.m_nBumpmap != -1) && params[info.m_nBumpmap]->IsDefined() )
	{
		pShader->LoadTexture( info.m_nBumpmap );
	}
}

void DrawPrototype( CBaseVSShader *pShader, IMaterialVar** params, IShaderDynamicAPI *pShaderAPI,
					IShaderShadow* pShaderShadow, PrototypeVars_t &info, VertexCompressionType_t vertexCompression )
{
	SHADOW_STATE
	{
		// Set stream format (note that this shader supports compression)
		unsigned int flags = VERTEX_POSITION | VERTEX_NORMAL | VERTEX_FORMAT_COMPRESSED;

		bool bHasVertexColor = IS_FLAG_SET( MATERIAL_VAR_VERTEXCOLOR );
		bool bHasVertexAlpha = IS_FLAG_SET( MATERIAL_VAR_VERTEXALPHA );
		if ( bHasVertexColor || bHasVertexAlpha )
			flags |= VERTEX_COLOR;

		int nTexCoordCount = 1;
		int userDataSize = 0;
		pShaderShadow->VertexShaderVertexFormat( flags, nTexCoordCount, NULL, userDataSize );

		// Vertex Shader
		DECLARE_STATIC_VERTEX_SHADER( prototype_vs20 );
		SET_STATIC_VERTEX_SHADER_COMBO( VERTEXCOLOR, bHasVertexColor || bHasVertexAlpha );
		SET_STATIC_VERTEX_SHADER( prototype_vs20 );
	
		// Pixel Shader
		DECLARE_STATIC_PIXEL_SHADER( prototype_ps20b );
		SET_STATIC_PIXEL_SHADER_COMBO( VERTEXCOLOR, bHasVertexColor || bHasVertexAlpha );
		SET_STATIC_PIXEL_SHADER( prototype_ps20b );

		// Textures
		pShaderShadow->EnableTexture( SHADER_SAMPLER0, true ); // Color texture
		pShaderShadow->EnableSRGBRead( SHADER_SAMPLER0, true );
		pShaderShadow->EnableTexture( SHADER_SAMPLER1, true ); // Bump
		pShaderShadow->EnableSRGBRead( SHADER_SAMPLER1, false ); // Not sRGB
		pShaderShadow->EnableSRGBWrite( true );

		// Blending
		//pShader->EnableAlphaBlending( SHADER_BLEND_SRC_ALPHA, SHADER_BLEND_ONE_MINUS_SRC_ALPHA );
		//pShaderShadow->EnableAlphaWrites( false );

		// !!! We need to turn this back on because EnableAlphaBlending() above disables it!
		//pShaderShadow->EnableDepthWrites( true );

		pShader->PI_BeginCommandBuffer();
		pShader->PI_SetPixelShaderAmbientLightCube( 3 );
		pShader->PI_SetPixelShaderLocalLighting( 9 );
		pShader->PI_SetVertexShaderAmbientLightCube();
		pShader->PI_EndCommandBuffer();
	}
	DYNAMIC_STATE
	{
		LightState_t lightState = { 0, false, false };

		// Set Vertex Shader Combos
		DECLARE_DYNAMIC_VERTEX_SHADER( prototype_vs20 );
		SET_DYNAMIC_VERTEX_SHADER_COMBO( SKINNING, pShaderAPI->GetCurrentNumBones() > 0 );
		SET_DYNAMIC_VERTEX_SHADER_COMBO( COMPRESSED_VERTS, (int)vertexCompression );
		SET_DYNAMIC_VERTEX_SHADER_COMBO( DYNAMIC_LIGHT, lightState.HasDynamicLight() );
		SET_DYNAMIC_VERTEX_SHADER_COMBO( STATIC_LIGHT, lightState.m_bStaticLight ? 1 : 0 );
		SET_DYNAMIC_VERTEX_SHADER_COMBO( NUM_LIGHTS, lightState.m_nNumLights );
		SET_DYNAMIC_VERTEX_SHADER( prototype_vs20 );

		// Set Vertex Shader Constants 
		pShaderAPI->GetDX9LightState( &lightState );

		// Set Pixel Shader Combos
		DECLARE_DYNAMIC_PIXEL_SHADER( prototype_ps20b );
		SET_DYNAMIC_PIXEL_SHADER_COMBO( NUM_LIGHTS, lightState.m_nNumLights );
		SET_DYNAMIC_PIXEL_SHADER_COMBO( AMBIENT_LIGHT, lightState.m_bAmbientLight ? 1 : 0 );
		SET_DYNAMIC_PIXEL_SHADER( prototype_ps20b );

		// Bind textures
		pShader->BindTexture( SHADER_SAMPLER0, info.m_nBaseTexture, info.m_nBaseTextureFrame );

		bool bBumpMapping = ( ( info.m_nBumpmap == -1 ) || !params[info.m_nBumpmap]->IsTexture() ) ? false : true;
		if ( bBumpMapping )
		{
			pShader->BindTexture( SHADER_SAMPLER1, info.m_nBumpmap, info.m_nBumpFrame );
		}

		// Set c0 and c1 to contain first two rows of ViewProj matrix
		VMatrix mView, mProj;
		pShaderAPI->GetMatrix( MATERIAL_VIEW, mView.m[0] );
		pShaderAPI->GetMatrix( MATERIAL_PROJECTION, mProj.m[0] );
		VMatrix mViewProj = mView * mProj;
		mViewProj = mViewProj.Transpose3x3();
		pShaderAPI->SetPixelShaderConstant( 0, mViewProj.m[0], 2 );

		// Camera pos
		float vEyePos[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
		pShaderAPI->GetWorldSpaceCameraPosition( vEyePos );
		pShaderAPI->SetPixelShaderConstant( 2, vEyePos, 1 );
	}
	pShader->Draw();
}
