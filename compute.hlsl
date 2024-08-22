
StructuredBuffer<int> srcBuffer : register(t0);
RWStructuredBuffer<int> dstBuffer : register(u0);

[numthreads(1024, 1, 1)]
void main(uint3 groupID : SV_GroupID, uint3 tid : SV_DispatchThreadID, uint3 localTID : SV_GroupThreadID, uint groupIndex : SV_GroupIndex)
{
    const int index = tid.x;

    dstBuffer[index] = srcBuffer[index] + 10;
}