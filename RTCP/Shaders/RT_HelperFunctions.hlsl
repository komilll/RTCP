#ifndef _RT_HELPER_FUNCTIONS_HLSL_
#define _RT_HELPER_FUNCTIONS_HLSL_

//// Retrieve attribute at a hit position interpolated from vertex attributes using the hit's barycentrics.
//float3 HitAttribute(float3 vertexAttribute[3], Attributes attrib)
//{
//	return vertexAttribute[0] +
//        attrib.bary.x * (vertexAttribute[1] - vertexAttribute[0]) +
//        attrib.bary.y * (vertexAttribute[2] - vertexAttribute[0]);
//}

//uint3 Load3x32BitIndices(uint offsetBytes)
//{
//	return indices.Load3(offsetBytes);
//}

//// Load three 16 bit indices from a byte addressed buffer.
//uint3 Load3x16BitIndices(uint offsetBytes)
//{
//	uint3 indices_;
//    // ByteAdressBuffer loads must be aligned at a 4 byte boundary.
//    // Since we need to read three 16 bit indices: { 0, 1, 2 } 
//    // aligned at a 4 byte boundary as: { 0 1 } { 2 0 } { 1 2 } { 0 1 } ...
//    // we will load 8 bytes (~ 4 indices { a b | c d }) to handle two possible index triplet layouts,
//    // based on first index's offsetBytes being aligned at the 4 byte boundary or not:
//    //  Aligned:     { 0 1 | 2 - }
//    //  Not aligned: { - 0 | 1 2 }
//	const uint dwordAlignedOffset = offsetBytes & ~3;
//	const uint2 four16BitIndices = indices.Load2(dwordAlignedOffset);
 
//    // Aligned: { 0 1 | 2 - } => retrieve first three 16bit indices
//	if (dwordAlignedOffset == offsetBytes)
//	{
//		indices_.x = four16BitIndices.x & 0xffff;
//		indices_.y = (four16BitIndices.x >> 16) & 0xffff;
//		indices_.z = four16BitIndices.y & 0xffff;
//	}
//	else // Not aligned: { - 0 | 1 2 } => retrieve last three 16bit indices
//	{
//		indices_.x = (four16BitIndices.x >> 16) & 0xffff;
//		indices_.y = four16BitIndices.y & 0xffff;
//		indices_.z = (four16BitIndices.y >> 16) & 0xffff;
//	}

//	return indices_;
//}

//// Generate a ray in world space for a camera pixel corresponding to an index from the dispatched 2D grid.
//inline void GenerateCameraRay(uint2 index, out float3 origin, out float3 direction)
//{
//	float2 xy = index + float2(0.5f, 0.5f); // center in the middle of the pixel.
//	float2 screenPos = xy / DispatchRaysDimensions().xy * 2.0 - 1.0;

//    // Invert Y for DirectX-style coordinates.
//	screenPos.y = -screenPos.y;

//    // Unproject the pixel coordinate into a ray.
//	float4 world = mul(float4(screenPos, 0, 1), g_sceneCB.projectionToWorld);

//	world.xyz /= world.w;
//	origin = g_sceneCB.cameraPosition.xyz;
//	direction = normalize(world.xyz - origin);
//}

//// Get hit position in world-space
//inline float3 HitWorldPosition()
//{
//	return WorldRayOrigin() + RayTCurrent() * WorldRayDirection();
//}

#endif // _RT_HELPER_FUNCTIONS_HLSL_
