#pragma once
// Authored host shader services. Original GIANTS shader injections are appended
// at runtime from the local game XML, not copied into the repository.
static constexpr const char* world_shader_services=R"(
cbuffer WorldLighting:register(b2){row_major float4x4 shadowVP;float4 sunDirection;float4 sunDiffuse;float4 ambientColor;float4 distanceFog;float4 frameInfo;};
Texture2D<float> shadowTexture:register(t10);SamplerComparisonState shadowSampler:register(s2);
float shadowVisibility(float3 world){
 if(frameInfo.y<0.5)return 1;
 float4 p=mul(float4(world,1),shadowVP);float3 q=p.xyz/p.w;float2 uv=q.xy*float2(0.5,-0.5)+0.5;
 if(any(uv<0)||any(uv>1)||q.z<0||q.z>1)return 1;
 float visible=0;
 [unroll]for(int y=-1;y<=1;++y)[unroll]for(int x=-1;x<=1;++x)visible+=shadowTexture.SampleCmpLevelZero(shadowSampler,uv+float2(x,y)*frameInfo.z,q.z-frameInfo.w);
 return visible/9;
}
float3 applyDistanceFog(float3 color,float3 world,float3 eye){float transmission=exp(-distanceFog.w*distance(world,eye));return lerp(distanceFog.xyz,color,transmission);}
)";
