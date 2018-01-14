// �n�b�V���̃v���W�F�N�V����

#include "const_buffer.hlsli"

#define kTileWidth				(16)
#define kTileSize				(kTileWidth * kTileWidth)

// �萔�o�b�t�@
cbuffer CbWaterInfo
{
	float	waterHeight;
};


// ����
Texture2D				texLinearDepth;

// �o��
RWTexture2D<uint>		rwProjectHash;

// ���[���h���W���擾����
float3 GetSurfaceWorldPos(uint2 uv)
{
	float linearDepth = texLinearDepth[uv].r;

	float2 screen_uv = (float2)uv / screenInfo.xy;
	screen_uv = screen_uv * float2(2, -2) + float2(-1, 1);
	float3 frustumVec = { frustumCorner.x * screen_uv.x, frustumCorner.y * screen_uv.y, -frustumCorner.z };
	float3 posVS = frustumVec * linearDepth;
	return mul(mtxViewToWorld, float4(posVS, 1)).xyz;
}


[numthreads(kTileWidth, kTileWidth, 1)]
void main(
	uint3 groupId          : SV_GroupID,
	uint3 dispatchThreadId : SV_DispatchThreadID,
	uint3 groupThreadId : SV_GroupThreadID)
{
	// �e�s�N�Z���̖@���A�[�x�A�A���x�h���擾����
	uint2 frameUV = dispatchThreadId.xy;
	float3 posWS = GetSurfaceWorldPos(frameUV);

	if (waterHeight >= posWS.y)
	{
		return;
	}

	// ���ʂɔ��˂������W�����߂�
	float3 posReflWS = float3(posWS.x, 2.0 * waterHeight - posWS.y, posWS.z);
	float4 posReflVS = mul(mtxWorldToView, float4(posReflWS, 1));
	float4 posReflCS = mul(mtxViewToClip, posReflVS);
	float2 posReflUV = posReflCS.xy / posReflCS.w * float2(0.5, -0.5) + 0.5;

	if (any(posReflUV < -1.0) || any(posReflUV > 1.0))
	{
		return;
	}

	// ���ː��UV�ɔ��ˌ���UV���i�[����
	uint2 reflFrameUV = (uint2)(posReflUV * screenInfo.xy);
	uint phash = frameUV.y << 16 | frameUV.x;
	InterlockedMax(rwProjectHash[reflFrameUV], phash);
}

//	EOF
