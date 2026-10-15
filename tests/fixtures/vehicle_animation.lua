-- Authored native-service fixture. Game code is loaded from local inputs only.
local root, mode = assert(arg[1]), assert(arg[2])
function source(path)
    local file=root..'/recovered/lua/'..path
    if mode=='original' and (path:match('/AnimatedVehicle.lua$') or path:match('/Trailer.lua$') or path:match('/SpecializationUtil.lua$')) then
        file=root..'/recovered/luajit/'..path:gsub('%.lua$','.luc')
    end
    return assert(loadfile(file))()
end
source('dataS/scripts/shared/class.lua')
source('dataS/scripts/Utils.lua')
source('dataS/scripts/SpecializationUtil.lua')
Event={};Class(Event)
function Event:new(mt) return setmetatable({},mt) end
EventIds=setmetatable({},{__index=function()return 0 end})
function InitStaticEventClass() end
g_server={broadcastEvent=function()end}
g_languageShort='en';g_i18n={getText=function(_,key)return key end,hasText=function()return false end}
Fillable={FILLTYPE_UNKNOWN=0,fillTypeNameToInt={wheat=1}}
source('dataS/scripts/vehicles/specializations/AnimatedVehicle.lua')
source('dataS/scripts/vehicles/specializations/Trailer.lua')
local nodes={}
local function node(i)
    nodes[i]=nodes[i] or {r={0,0,0},t={0,0,0},s={1,1,1},visible=true}
    return nodes[i]
end
function getRotation(i)return unpack(node(i).r)end
function getTranslation(i)return unpack(node(i).t)end
function getScale(i)return unpack(node(i).s)end
function getVisibility(i)return node(i).visible end
function getName(i)return 'fixture-'..i end
function setRotation(i,...)node(i).r={...}end
function setTranslation(i,...)node(i).t={...}end
function setScale(i,...)node(i).s={...}end
function setVisibility(i,v)node(i).visible=v end
function getWorldTranslation(i)return getTranslation(i)end
function localDirectionToWorld(_,x,y,z)return x,y,z end
Utils.indexToObject=function(_,index)return index=='0>' and 1 or index and tonumber(index)end
function getXMLString(xml,key)local v=xml[key];return v~=nil and tostring(v) or nil end
function getXMLFloat(xml,key)return tonumber(xml[key])end
getXMLInt=getXMLFloat
function getXMLBool(xml,key)return xml[key]end
function hasXMLProperty(xml,key)return xml[key]~=nil end
local xml={}
local function animation(i,name,parts,looping)
    local k='vehicle.animations.animation('..i..')'
    xml[k]=true;xml[k..'#name']=name;xml[k..'#looping']=looping
    for j,part in ipairs(parts)do
        local p=k..'.part('..(j-1)..')';xml[p]=true
        for attr,value in pairs(part)do xml[p..'#'..attr]=value end
    end
end
animation(0,'sequence',{
    {node='1',startTime=0,endTime=1,startRot='0 0 0',endRot='90 0 0'},
    {node='1',startTime=1,endTime=2,endRot='0 45 0'},
    {node='2',startTime=0.25,duration=1,startTrans='2 3 4',endTrans='4 7 8'},
    {node='2',startTime=1.25,duration=0.75,endScale='2 3 4'},
    {node='3',startTime=0,endTime=0.5,visibility=false},
    {node='3',startTime=0.5,endTime=2,visibility=true}
})
animation(1,'tip',{{node='4',startTime=0,endTime=3,startRot='0 0 0',endRot='40 0 0'}})
animation(2,'door',{{node='5',startTime=0,endTime=1,startRot='0 0 0',endRot='0 80 0'}})
animation(3,'limits',{{componentJointIndex=0,startTime=0,endTime=1,startRotLimit='0 0 0',endRotLimit='10 20 30',startTransLimit='0 0 0',endTransLimit='1 2 3'}})
animation(4,'loop',{{node='6',startTime=0,endTime=0.2,startTrans='0 0 0',endTrans='1 0 0'}},true)
local tip='vehicle.tipAnimations.tipAnimation(0)'
xml['vehicle.tipAnimations']=true;xml[tip]=true
for key,value in pairs({animationName='tip',doorAnimationName='door',dischargeStartTime=1,dischargeEndTime=3,speedScale=1,closeSpeedScale=-2})do xml[tip..'#'..key]=value end
local ref='vehicle.tipReferencePoints.tipReferencePoint(0)';xml[ref]=true;xml[ref..'#index']='4'
local function vehicle()
    nodes={};node(2).t={2,3,4}
    local v={components={},componentJoints={{rotLimit={0,0,0},transLimit={0,0,0}}},
        specializations={AnimatedVehicle,Trailer},isServer=true,isClient=false,configFileName='authored fixture',
        fillLevel=300,capacity=300,currentFillType=1,fillVolumeHeights={}}
    function v:setComponentJointRotLimit(joint,i,_,limit)joint.rotLimit[i]=limit end
    function v:setComponentJointTransLimit(joint,i,_,limit)joint.transLimit[i]=limit end
    function v:getCapacity()return self.capacity end
    function v:setFillLevel(level,kind)self.fillLevel=math.max(0,math.min(level,self.capacity));self.currentFillType=self.fillLevel==0 and 0 or kind end
    AnimatedVehicle.load(v,xml)
    Trailer.load(v,xml)
    v:getAnimationTime('sequence') -- Recovered specialization dispatcher is exercised.
    for _,anim in ipairs(v.tipAnimations)do anim.dischargeParticleSystems={} end
    v.getCanTipToGround=function()return false end
    return v
end
local samples=0
local function trace(label,v)
    samples=samples+1
    local parts={label,string.format('%.9f',v:getAnimationTime('sequence')),string.format('%.9f',v:getAnimationTime('tip')),tostring(v.tipState),string.format('%.9f',v.fillLevel)}
    for i=1,6 do
        local n=node(i)
        for _,p in ipairs({n.r,n.t,n.s})do for _,value in ipairs(p)do parts[#parts+1]=string.format('%.9f',value)end end
        parts[#parts+1]=tostring(n.visible)
    end
    for _,p in ipairs({v.componentJoints[1].rotLimit,v.componentJoints[1].transLimit})do for _,value in ipairs(p)do parts[#parts+1]=string.format('%.9f',value)end end
    print(table.concat(parts,'|'))
end
local v=vehicle()
v:playAnimation('sequence',1,0,true)
for i=1,150 do AnimatedVehicle.update(v,16);trace('forward',v)end
assert(math.abs(node(1).r[2]-math.rad(45))<0.0001 and not v:getIsAnimationPlaying('sequence'))
v:playAnimation('sequence',-2,1,true)
for i=1,80 do AnimatedVehicle.update(v,16);trace('reverse',v)end
assert(math.abs(node(1).r[2])<0.0001 and v:getAnimationTime('sequence')==0)
v:playAnimation('sequence',1,0,true);v:setAnimationStopTime('sequence',0.35)
for i=1,55 do AnimatedVehicle.update(v,16);trace('stop',v)end
assert(math.abs(v:getAnimationTime('sequence')-0.35)<0.00001 and not v:getIsAnimationPlaying('sequence'))
v:setAnimationTime('sequence',0.9,true);trace('seek-forward',v)
v:setAnimationTime('sequence',0.2,true);trace('seek-reverse',v)
v:playAnimation('limits',1,0,true)
for i=1,80 do AnimatedVehicle.update(v,16);trace('joint-limits',v)end
assert(math.abs(v.componentJoints[1].transLimit[3]-3)<0.0001)
v:playAnimation('loop',1,0,true)
for i=1,50 do AnimatedVehicle.update(v,16);trace('loop',v)end
assert(v:getIsAnimationPlaying('loop'));v:stopAnimation('loop',true)
v=vehicle()
local received=0
local trigger={updateTrailerTipping=function(_,_,delta)received=received-delta end}
g_currentMission={getIsTrailerInTipRange=function()return true end}
v:onStartTip(trigger,1,true,true)
for i=1,360 do
    AnimatedVehicle.update(v,16);Trailer.updateTick(v,16);trace('tip-cycle',v)
    assert(math.abs(v.fillLevel+received-300)<0.000001)
end
assert(math.abs(received-300)<0.000001 and v.tipState==Trailer.TIPSTATE_CLOSED and v:getCurrentTipAnimationTime()==0)
print('PASS samples='..samples)
