-- Authored single-player service adapter around recovered FS15 classes.
-- The planar tyre/contact integrator below is temporary, not GE6/PhysX parity.
source('dataS/scripts/shared/class.lua')
source('dataS/scripts/Utils.lua')
source('dataS/scripts/AnimCurve.lua')
Object, Event = {}, {}
Class(Object); Class(Event)
function Object:new(server, client, mt)
    return setmetatable({isServer=server, isClient=client}, mt)
end
function Object:getNextDirtyFlag() return 1 end
function Object:raiseDirtyFlags(_) end -- Single local authority, no replication.
function Event:new(mt) return setmetatable({}, mt) end
ObjectIds = {OBJECT_MULTI_SILO_TRIGGER=1}
EventIds = setmetatable({}, {__index=function() return 0 end})
function InitStaticObjectClass() end
function InitStaticEventClass() end
g_server = {broadcastEvent=function() end} -- Network transport intentionally absent.
g_i18n = {getText=function(_, key) return key end, hasText=function() return false end}
source('dataS/scripts/vehicles/specializations/Fillable.lua')
source('dataS/scripts/FruitUtil.lua')
source('dataS/scripts/vehicles/VehicleMotor.lua')
source('dataS/scripts/vehicles/WheelsUtil.lua')
source('dataS/scripts/vehicles/specializations/Motorized.lua')
source('dataS/scripts/triggers/MultiSiloTrigger.lua')
source('dataS/scripts/objects/FieldDefinition.lua')
source('dataS/scripts/FSUtils.lua')
source('dataS/scripts/triggers/TipTrigger.lua')
source('dataS/scripts/vehicles/specializations/Trailer.lua')
source('dataS/scripts/vehicles/specializations/Overloading.lua')
source('dataS/scripts/vehicles/specializations/SetIsFillingEvent.lua')
source('dataS/scripts/triggers/FillTrigger.lua')

local function originalSlice(path, first, after, prefix, suffix)
    local text = worldReadSource(path)
    local start = assert(text:find(first, 1, true), 'Recovered source start missing')
    local finish = assert(text:find(after, start+#first, true), 'Recovered source end missing')
    return assert(loadstring((prefix or '')..text:sub(start, finish-1)..(suffix or ''), '@'..path..':recovered-slice'))()
end
originalSlice('dataS/scripts/main.lua', 'local fruitWeightScale = 0.5', 'BaleUtil.registerBaleType(')
FSBaseMission = {}
originalSlice('dataS/scripts/FSBaseMission.lua', 'function FSBaseMission:getSiloAmount(', 'function FSBaseMission:addSharedMoney(')
BaseMission={}
originalSlice('dataS/scripts/BaseMission.lua','function BaseMission:getIsTrailerInTipRange(','function BaseMission:getSteerableInRange(')

-- Original motor calls use the no-PTO/no-differential branch until native joints
-- and drivetrain services are reconstructed. These return actual current state.
PowerConsumer = {getTotalConsumedPtoTorque=function() return 0 end, getMaxPtoRpm=function() return 0 end}
local wheelsByShape = {}
function getWheelShapeAxleSpeed(_, shape)
    local w = assert(wheelsByShape[shape])
    return w.vehicle.speed / w.radius
end
function setWheelShapeProps(_, shape, torque, brake, steering)
    local w = assert(wheelsByShape[shape])
    w.torque, w.brake, w.steeringAngle = torque, brake, steering
end

local vehicles, silos, triggers, tipTriggers, fillTriggers = {}, {}, {}, {}, {}
local totalDistance, transferred, unloaded, selection, freeCamera = 0, 0, 0, 0, false
local harvested,worked,seeded=0,0,0
local checks={}
local message = ''
g_gui = {}
g_currentMission = setmetatable({
    objectToTrailer={}, nodeToVehicle={}, controlledVehicle=nil, missionStats={farmSiloAmounts={}}, siloAmountListeners={},
    missionInfo={}, ingameMap={createMapHotspot=function() return {} end}, fields={},
    getIsServer=function() return true end, getIsClient=function() return false end,
    addActivatableObject=function() end, removeActivatableObject=function() end,
    addNonUpdateable=function() end,
    addTipTrigger=function(_,trigger) tipTriggers[#tipTriggers+1]=trigger end,
    setIsFieldOwnedAtWorldParallelogram=function(self,x,z,x1,z1,x2,z2,owned)
        self.fields[#self.fields+1]={x,z,x1,z1,x2,z2,owned=owned}
    end
}, {__index=FSBaseMission})
g_currentMission.missionStats.difficulty=worldDifficulty
g_currentMission.tipTriggerRangeThreshold=1
g_currentMission.terrainDetailId=1
g_currentMission.dynamicFoliageLayers={}
originalSlice('dataS/scripts/FSBaseMission.lua','self.cultivatorChannel = 0','self:updateFoliageGrowthStateTime()','local self=g_currentMission\n')
g_currentMission.fruits={}
for _,layer in ipairs(worldCropLayers()) do
    local desc=FruitUtil.fruitTypes[layer.name]
    if desc then g_currentMission.fruits[desc.index]={id=layer.id,windrowId=layer.id+100} end
end
g_currentMission.getIsTrailerInTipRange=BaseMission.getIsTrailerInTipRange
g_currentMission.economyManager={getCurrentGreatDemand=function() return nil end}
g_currentMission.money=1000 -- FSMissionInfo:loadDefaults / CareerScreen.defaultMoney.
g_currentMission.missionStats.expenses=0
g_currentMission.missionStats.updateStats=function(self,name,amount) self[name]=(self[name] or 0)+amount end
g_currentMission.addSharedMoney=function(self,amount) self.money=self.money+amount end
g_currentMission.addMoneyChange=function() end -- Money notification UI pending.
g_currentMission.showMoneyChange=function() end
local nextMoneyId=0
function getMoneyTypeId() nextMoneyId=nextMoneyId+1;return nextMoneyId end
local defaults=originalSlice('dataS/scripts/FSMissionInfo.lua','self.farmSiloAmounts = {}','self.fillTypePrices = {}','local self={}\n','\nreturn self')
for name, amount in pairs(defaults.farmSiloAmounts) do
    local fill=Fillable.fillTypeNameToInt[name]
    if fill then g_currentMission:setSiloAmount(fill,amount) end
end
function addTrigger(id, callback, object) triggers[id]={callback=callback, object=object} end
function removeTrigger(id) triggers[id]=nil end
function setVisibility() end -- Server-side field marker only; visual service pending.

local function number(xml, key, default) return Utils.getNoNil(getXMLFloat(xml, key), default) end
local function point(node) return {getWorldTranslation(node)} end
local function pose(v)
    worldSetPose(v.id,v.x,v.y,v.z,v.pitch or 0,v.yaw,v.roll or 0)
end
local function rootVehicle(self) return self.attacherVehicle or self end
local function noop() end
local function modelAnimation(v,name,time)
    local anim=v.animations[name]
    if not anim then return end
    local seconds=time*anim.duration
    for _,part in ipairs(anim.parts) do
        local t=Utils.clamp((seconds-part.start)/math.max(part.finish-part.start,0.001),0,1)
        if part.r0 and part.r1 then setRotation(part.node,Utils.vector3ArrayLerp(part.r0,part.r1,t)) end
        if part.t0 and part.t1 then setTranslation(part.node,Utils.vector3ArrayLerp(part.t0,part.t1,t)) end
    end
end
local function loadVehicle(config, template, key)
    local xml = assert(loadXMLFile('VehicleConfig', config))
    local id, roots = worldLoadVehicle(config)
    local v = {id=id,config=config,name=getXMLString(xml,'vehicle.name.en') or config,
        components={},wheels={},differentials={},attachedImplements={},speed=0,movingDirection=1,
        lastSpeedReal=0,rotatedTime=0,isServer=true,isClient=false,
        setBrakeLightsVisibility=noop,setReverseLightsVisibility=noop,
        getRootAttacherVehicle=rootVehicle,onBrake=noop,onReleaseBrake=noop,
        fillVolumes={},fillLevel=0,currentFillType=0,lastValidFillType=0,fillTypeChangeThreshold=0.05,
        fillTypes={[0]=true},allowFillFromAir=true,measurementTime=0,fillTriggers={},fillActivatable={},isFilling=false}
    local count = getXMLInt(xml,'vehicle.components#count') or 1
    for i=1,count do v.components[i]={node=assert(roots[i])} end
    v.rootNode=roots[1]
    for name, fn in pairs(Fillable) do if type(fn)=='function' then v[name]=fn end end
    v.mass=0
    for i=1,count do v.mass=v.mass+number(xml,'vehicle.components.component'..i..'#mass',1) end
    v.capacity=number(xml,'vehicle.capacity',0)
    v.allowFillFromAir=Utils.getNoNil(getXMLBool(xml,'vehicle.allowFillFromAir#value'),true)
    for word in ((getXMLString(xml,'vehicle.fillTypes#fruitTypes') or '')..' '..(getXMLString(xml,'vehicle.fillTypes#fillTypes') or '')):gmatch('%S+') do
        local fill=Fillable.fillTypeNameToInt[word]
        if fill then v.fillTypes[fill]=true end
    end
    local fillIndex=getXMLString(xml,'vehicle.fillVolumes.fillVolume(0)#index')
    if fillIndex then v.fillNode=Utils.indexToObject(v.components,fillIndex);v.exactFillRootNode=v.fillNode;g_currentMission.objectToTrailer[v.fillNode]=v;g_currentMission.nodeToVehicle[v.fillNode]=v end
    if not v.fillNode and v.capacity>0 then v.fillNode=v.rootNode;v.exactFillRootNode=v.fillNode;g_currentMission.objectToTrailer[v.fillNode]=v;g_currentMission.nodeToVehicle[v.fillNode]=v end
    v.fillLitersPerSecond=number(xml,'vehicle.fillLitersPerSecond',500)
    local i=0
    while hasXMLProperty(xml,'vehicle.wheels.wheel('..i..')') do
        local k='vehicle.wheels.wheel('..i..')'
        local repr=assert(Utils.indexToObject(v.components,getXMLString(xml,k..'#repr')))
        local drive=Utils.indexToObject(v.components,getXMLString(xml,k..'#driveNode')) or repr
        local p=point(repr)
        local w={vehicle=v,repr=repr,drive=drive,node=roots[1],wheelShape=id*100+i,radius=number(xml,k..'#radius',0.5),
            rotSpeed=number(xml,k..'#rotSpeed',0),rotMin=-math.rad(42),rotMax=math.rad(42),
            steeringAxleScale=0,hasHandbrake=true,hasGroundContact=true,driveMode=0,steeringAngle=0,
            positionX=p[1],positionY=p[2],positionZ=p[3],baseRotation={getRotation(drive)},reprRotation={getRotation(repr)},spin=0}
        v.wheels[#v.wheels+1]=w;wheelsByShape[w.wheelShape]=w
        i=i+1
    end
    v.restHeight=0
    local front, rear=-math.huge,math.huge
    for _,w in ipairs(v.wheels) do v.restHeight=math.max(v.restHeight,w.radius-w.positionY);front=math.max(front,w.positionZ);rear=math.min(rear,w.positionZ) end
    v.wheelbase=math.max(front-rear,1)
    if getXMLFloat(xml,'vehicle.motor#minRpm') then Motorized.loadMotor(v,xml) end
    v.joints={}
    i=0
    while hasXMLProperty(xml,'vehicle.attacherJoints.attacherJoint('..i..')') do
        local k='vehicle.attacherJoints.attacherJoint('..i..')'
        v.joints[#v.joints+1]={node=Utils.indexToObject(v.components,getXMLString(xml,k..'#index')),type=getXMLString(xml,k..'#jointType') or 'implement'}
        i=i+1
    end
    local input=getXMLString(xml,'vehicle.inputAttacherJoints.inputAttacherJoint(0)#index')
    if input then v.inputJoint=Utils.indexToObject(v.components,input);v.jointType=getXMLString(xml,'vehicle.inputAttacherJoints.inputAttacherJoint(0)#jointType') or 'implement' end
    v.tipReferencePoints={}
    i=0
    while hasXMLProperty(xml,'vehicle.tipReferencePoints.tipReferencePoint('..i..')') do
        local k='vehicle.tipReferencePoints.tipReferencePoint('..i..')'
        v.tipReferencePoints[#v.tipReferencePoints+1]={node=Utils.indexToObject(v.components,getXMLString(xml,k..'#index')),width=number(xml,k..'#width',0)}
        i=i+1
    end
    v.tipAnimations={}
    i=0
    while hasXMLProperty(xml,'vehicle.tipAnimations.tipAnimation('..i..')') do
        local k='vehicle.tipAnimations.tipAnimation('..i..')'
        v.tipAnimations[#v.tipAnimations+1]={dischargeStartTime=number(xml,k..'#dischargeStartTime',0)*1000,
            dischargeEndTime=number(xml,k..'#dischargeEndTime',10)*1000,animationOpenSpeedScale=number(xml,k..'#speedScale',1)}
        i=i+1
    end
    v.currentTipReferencePointIndex=1;v.fillLevelToTippedFillLevel=1
    v.workAreas={};i=0
    while hasXMLProperty(xml,'vehicle.workAreas.workArea('..i..')') do
        local k='vehicle.workAreas.workArea('..i..')'
        local area={kind=getXMLString(xml,k..'#type'),start=Utils.indexToObject(v.components,getXMLString(xml,k..'#startIndex')),
            width=Utils.indexToObject(v.components,getXMLString(xml,k..'#widthIndex')),height=Utils.indexToObject(v.components,getXMLString(xml,k..'#heightIndex'))}
        if area.start and area.width and area.height then v.workAreas[#v.workAreas+1]=area end
        i=i+1
    end
    v.acceptedFruits={}
    for name in (getXMLString(xml,'vehicle.fruitTypes#fruitTypes') or ''):gmatch('%S+') do v.acceptedFruits[name]=true end
    v.animations={};v.playing={};i=0
    while hasXMLProperty(xml,'vehicle.animations.animation('..i..')') do
        local k='vehicle.animations.animation('..i..')'
        local anim={parts={},duration=0};local j=0
        while hasXMLProperty(xml,k..'.part('..j..')') do
            local p=k..'.part('..j..')';local start=number(xml,p..'#startTime',0)
            local part={node=Utils.indexToObject(v.components,getXMLString(xml,p..'#node')),start=start,
                finish=number(xml,p..'#endTime',start+number(xml,p..'#duration',1))}
            for src,dst in pairs({startRot='r0',endRot='r1',startTrans='t0',endTrans='t1'}) do
                local str=getXMLString(xml,p..'#'..src)
                if str then local values={Utils.getVectorFromString(str)};if src:find('Rot') then for n=1,3 do values[n]=math.rad(values[n]) end end;part[dst]=values end
            end
            if part.node then anim.parts[#anim.parts+1]=part;anim.duration=math.max(anim.duration,part.finish) end
            j=j+1
        end
        v.animations[getXMLString(xml,k..'#name')]=anim;i=i+1
    end
    v.playAnimation=function(self,name,speed,time) self.playing[name]={speed=speed,time=time or 0} end
    v.getAnimationTime=function(self,name) return self.playing[name] and self.playing[name].time or 0 end
    v.getNextDirtyFlag=function() return 1 end;v.raiseDirtyFlags=noop
    local overloading=getXMLFloat(xml,'vehicle.overloadingCapacity')
    if overloading then
        for _,name in ipairs({'setPipeState','setOverloadingActive','getIsPipeUnloadingAllowed','findTrailerToUnload','findTrailerRaycastCallback','activatePipeUnloadingEffects'}) do v[name]=Overloading[name] end
        v.hasPipe=true;v.numPipeStates=getXMLInt(xml,'vehicle.pipe#numStates') or 2;v.targetPipeState=1;v.currentPipeState=1
        v.pipeStateIsUnloading={[2]=true};v.pipeStateIsAutoAiming={};v.pipeUnloadEffectStopTime=0;v.currentUnloadEffectStopTime=0;v.overloadingDirtyFlag=1
        v.pipeAnimation=getXMLString(xml,'vehicle.pipe#animName');v.pipeAnimSpeedScale=number(xml,'vehicle.pipe#animSpeedScale',1)
        v.pipeRaycastNode=Utils.indexToObject(v.components,getXMLString(xml,'vehicle.pipe#raycastNodeIndex'))
        v.pipeRaycastDistance=number(xml,'vehicle.pipe#raycastDistance',6);v.overloadingCapacity=overloading
        for tag,field in pairs({fillVolumeDischargeInfo='fillVolumeDischargeInfo',fillVolumeUnloadInfo='fillVolumeUnloadInfo'}) do
            v[field]={node=Utils.indexToObject(v.components,getXMLString(xml,'vehicle.'..tag..'#index')),width=number(xml,'vehicle.'..tag..'#width',0.4),length=number(xml,'vehicle.'..tag..'#length',0.4)}
        end
    end
    v.getCanTip=Trailer.getCanTip;v.getCanTipToGround=function() return false end
    v.getCurrentTipAnimationTime=function(self) return self.tipTime or 0 end
    v.onEndTip=function(self) self.tipping=false end
    Fillable.postLoad(v,xml)
    local absolute=getXMLString(template,key..'.component1#position')
    if absolute then
        v.x,v.y,v.z=Utils.getVectorFromString(absolute)
        v.pitch,v.yaw,v.roll=Utils.getVectorFromString(getXMLString(template,key..'.component1#rotation'))
        -- Recover heading from the full original component orientation.
        pose(v)
        local dx,_,dz=localDirectionToWorld(v.rootNode,0,0,1)
        v.yaw=math.atan2(dx,dz);v.pitch,v.roll=0,0
    else
        v.x=number(template,key..'#xPosition',0);v.z=number(template,key..'#zPosition',0)
        v.y=worldHeight(v.x,v.z)+v.restHeight
        v.yaw=math.rad(number(template,key..'#yRotation',0))
    end
    pose(v);vehicles[#vehicles+1]=v;delete(xml)
    return v
end
local template=assert(loadXMLFile('StartingVehicles','profileTemplate/careerVehicles_'..worldMap:lower()..'.xml'))
local index=0
while hasXMLProperty(template,'careerVehicles.vehicle('..index..')') do
    local key='careerVehicles.vehicle('..index..')'
    loadVehicle(assert(getXMLString(template,key..'#filename')),template,key)
    index=index+1
end
delete(template)
for _,id in ipairs(worldFindCallbacks('MultiSiloTrigger.onCreate')) do
    local silo=MultiSiloTrigger:new(true,false);silo:load(id);silos[#silos+1]=silo
end
for _,id in ipairs(worldFindCallbacks('FieldDefinition.onCreate')) do
    FieldDefinition:onCreate(id);g_currentMission.fieldDefinitionBase:init()
    -- GE6 initializes the ownership bitmap to true, then clears unowned fields.
    -- Our polygon adapter also records the owned polygons explicitly.
    for _,field in ipairs(g_currentMission.fieldDefinitionBase.fieldDefs) do
        if field.ownedByPlayer then g_currentMission.fieldDefinitionBase:updateFieldOwnedData(field) end
    end
end
for _,id in ipairs(worldFindCallbacks('TipTrigger.onCreate')) do
    local trigger=TipTrigger:new(true,false);trigger:load(id)
end
for _,id in ipairs(worldFindCallbacks('FillTrigger.onCreate')) do
    local trigger=FillTrigger:new();trigger:load(id)
    if trigger.fillType~=Fillable.FILLTYPE_UNKNOWN then fillTriggers[#fillTriggers+1]=trigger end
end

local function selectVehicle()
    for _=1,#vehicles do
        selection=selection%#vehicles+1
        if vehicles[selection].motor then
            g_currentMission.controlledVehicle=vehicles[selection]
            worldSelect(freeCamera and 0 or selection)
            return
        end
    end
end
selectVehicle()

local contacts={}
local function updateSilos(dt)
    for _,silo in ipairs(silos) do
        for _,trailer in ipairs(vehicles) do if trailer.fillNode then
            for _,trigger in ipairs(silo.triggerIds) do
                local key=trigger..':'..trailer.id
                local inside=worldOverlap(trigger,trailer.fillNode)
                if inside~=not not contacts[key] then
                    silo:triggerCallback(trigger,trailer.rootNode,inside,not inside,false,trailer.fillNode)
                    contacts[key]=inside
                end
            end
        end end
        local before=silo.siloTrailer and silo.siloTrailer.fillLevel or 0
        silo:update(dt*1000)
        local after=silo.siloTrailer and silo.siloTrailer.fillLevel or before
        transferred=transferred+math.max(0,after-before)
    end
    for _,trigger in ipairs(fillTriggers) do for _,tool in ipairs(vehicles) do if tool.fillNode then
        local key='supply:'..trigger.triggerId..':'..tool.id
        local inside=worldOverlap(trigger.triggerId,tool.fillNode)
        if inside~=not not contacts[key] then
            trigger:triggerCallback(trigger.triggerId,tool.rootNode,inside,not inside,false,tool.fillNode)
            contacts[key]=inside
        end
    end end end
end
local function updateWheels(v,dt)
    for _,w in ipairs(v.wheels) do
        w.spin=w.spin+v.speed*dt/w.radius
        local a=w.baseRotation
        if w.drive==w.repr then setRotation(w.drive,a[1]+w.spin,a[2]+w.steeringAngle,a[3])
        else setRotation(w.drive,a[1]+w.spin,a[2],a[3]);local b=w.reprRotation;setRotation(w.repr,b[1],b[2]+w.steeringAngle,b[3]) end
    end
end
local function owned(x,z)
    for _,field in ipairs(g_currentMission.fields) do if field.owned then
        local wx,wz,hx,hz=field[3]-field[1],field[4]-field[2],field[5]-field[1],field[6]-field[2]
        local det=wx*hz-wz*hx
        if math.abs(det)>0.001 then
            local dx,dz=x-field[1],z-field[2]
            local u,t=(dx*hz-dz*hx)/det,(wx*dz-wz*dx)/det
            if u>=0 and u<=1 and t>=0 and t<=1 then return true end
        end
    end end
    return false
end
local function work(v)
    local tool=v.implement
    if not tool or not tool.working or math.abs(v.speed)<0.1 then return end
    for _,area in ipairs(tool.workAreas) do
        local x,_,z=getWorldTranslation(area.start)
        local x1,_,z1=getWorldTranslation(area.width)
        local x2,_,z2=getWorldTranslation(area.height)
        if owned((x1+x2)/2,(z1+z2)/2) then
            message='Working '..area.kind
            if area.kind=='cutter' then
                local name=worldCropInfo((x1+x2)/2,(z1+z2)/2)
                local desc=name and FruitUtil.fruitTypes[name]
                local fill=name and Fillable.fillTypeNameToInt[name]
                if desc and tool.acceptedFruits[name] and v.capacity>v.fillLevel+20 and v:allowFillType(fill,false) then
                    local pixels=Utils.cutFruitArea(desc.index,x,z,x1,z1,x2,z2,true,true)
                    local liters=pixels*0.25*desc.literPerSqm
                    local before=v.fillLevel;v:setFillLevel(v.fillLevel+liters,fill)
                    harvested=harvested+v.fillLevel-before
                end
            elseif area.kind=='cultivator' then
                local _,pixels=Utils.updateCultivatorArea(x,z,x1,z1,x2,z2,false,false,0)
                worked=worked+pixels*0.25
            elseif area.kind=='sowingMachine' and tool.fillLevel>0 then
                local pixels=Utils.updateSowingArea(FruitUtil.FRUITTYPE_WHEAT,x,z,x1,z1,x2,z2,0,false)
                local used=pixels*0.25*FruitUtil.fruitIndexToDesc[FruitUtil.FRUITTYPE_WHEAT].seedUsagePerSqm
                tool:setFillLevel(tool.fillLevel-used,Fillable.FILLTYPE_SEEDS);seeded=seeded+pixels*0.25
            end
        else message='This field is not owned' end
    end
end
function missionUpdate(dt,throttle,steering,handbrake)
    for _,v in ipairs(vehicles) do if v.motor then
        local selected=v==g_currentMission.controlledVehicle and not freeCamera
        local throttle,steering,handbrake=selected and throttle or 0,selected and steering or 0,not selected or handbrake
        v.rotatedTime=Utils.clamp(v.rotatedTime+steering*dt*1.5,-math.rad(42),math.rad(42))
        if math.abs(steering)<0.01 then v.rotatedTime=v.rotatedTime*math.max(0,1-dt*3) end
        v.lastSpeedReal=math.abs(v.speed)*0.001;v.movingDirection=v.speed<0 and -1 or 1
        for _,w in ipairs(v.wheels) do WheelsUtil.updateWheelSteeringAngle(v,w,dt*1000) end
        WheelsUtil.updateWheelsPhysics(v,dt*1000,math.abs(v.speed)*0.001,throttle,handbrake,0)
        -- Temporary engine primitive: planar force integration in tonnes/kN.
        -- Original WheelUtil/VehicleMotor choose torque, ratios and braking.
        local force,brake,angle,n=0,0,0,0
        for _,w in ipairs(v.wheels) do force=force+(w.torque or 0)/w.radius;brake=brake+(w.brake or 0);if w.rotSpeed~=0 then angle=angle+w.steeringAngle;n=n+1 end end
        local mass=v.mass+(v.implement and (v.implement.mass+v.implement.fillLevel*0.0004) or 0)
        v.speed=v.speed+force/math.max(mass,1)*dt
        local drag=(brake/math.max(mass,1)+0.08)*dt
        v.speed=math.max(0,math.abs(v.speed)-drag)*(v.speed<0 and -1 or 1)
        v.speed=Utils.clamp(v.speed,-v.motor.maxBackwardSpeed,v.motor.maxForwardSpeed)
        v.yaw=v.yaw+(n>0 and math.tan(angle/n) or 0)*v.speed/v.wheelbase*dt
        local step=v.speed*dt
        local nx,nz,blocked=worldMove(v.x,v.z,Utils.clamp(v.x+math.sin(v.yaw)*step,-1020,1020),Utils.clamp(v.z+math.cos(v.yaw)*step,-1020,1020),0.85)
        totalDistance=totalDistance+math.sqrt((nx-v.x)^2+(nz-v.z)^2);v.x,v.z=nx,nz
        if blocked then v.speed=0;message='Static obstacle' end
        v.y=worldHeight(v.x,v.z)+v.restHeight
        updateWheels(v,dt);pose(v)
        if v.implement then
            local t=v.implement
            local jx,jy,jz=getWorldTranslation(v.hitch)
            local tx,ty,tz=getWorldTranslation(t.inputJoint)
            t.yaw=v.yaw;pose(t);tx,ty,tz=getWorldTranslation(t.inputJoint)
            t.x=t.x+jx-tx;t.z=t.z+jz-tz;t.y=#t.wheels>0 and worldHeight(t.x,t.z)+t.restHeight or t.y+jy-ty;t.speed=v.speed
            updateWheels(t,dt);pose(t)
        end
        work(v)
    end end
    updateSilos(dt)
    for _,body in ipairs(vehicles) do
        Fillable.updateTick(body,dt*1000)
        for name,playing in pairs(body.playing) do
            local anim=body.animations[name]
            if anim and anim.duration>0 then
                playing.time=Utils.clamp(playing.time+playing.speed*dt/anim.duration,0,1)
                modelAnimation(body,name,playing.time)
                if name==body.pipeAnimation and (playing.time==1 or playing.time==0) then body.currentPipeState=body.targetPipeState end
            end
        end
        if body.hasPipe then pose(body);Overloading.updateTick(body,dt*1000) end
    end
    for _,t in ipairs(vehicles) do if t.tipping then
        t.tipTime=t.tipTime+dt*1000
        local before=t.fillLevel
        Trailer.updateTipping(t,dt*1000)
        unloaded=unloaded+math.max(0,before-t.fillLevel)
        if t.fillLevel==0 then t.tipping=false end
    end end
end
function missionAction(action)
    local v=g_currentMission.controlledVehicle
    if action=='selfcheck' then missionSelfcheck()
    elseif action=='scenario-harvest' or action=='scenario-silo' then missionScenario(action)
    elseif action=='next' then selectVehicle()
    elseif action=='camera' then freeCamera=not freeCamera;worldSelect(freeCamera and 0 or selection)
    elseif action=='work' and v and v.implement then v.implement.working=not v.implement.working;message=v.implement.working and 'Tool working' or 'Tool stopped'
    elseif action=='pipe' and v and v.hasPipe then v:setPipeState(v.targetPipeState==1 and 2 or 1);message='Pipe moving'
    elseif action=='attach' and v then
        if v.implement then v.implement.attacherVehicle=nil;v.implement=nil;message='Detached'
        else
            local best,distance,joint
            for _,t in ipairs(vehicles) do if t~=v and t.inputJoint and not t.attacherVehicle then
                local tx,_,tz=getWorldTranslation(t.inputJoint)
                for _,j in ipairs(v.joints) do if j.node and j.type==t.jointType then
                    local x,_,z=getWorldTranslation(j.node);local d=(x-tx)^2+(z-tz)^2
                    if d<9 and (not distance or d<distance) then best,distance,joint=t,d,j.node end
                end end
            end end
            if best then v.implement=best;v.hitch=joint;best.attacherVehicle=v;message='Attached '..best.name else message='Move a compatible hitch within 3 m' end
        end
    elseif action=='fill' then
        message='Place the trailer under the farm silo'
        local tool=v and v.implement
        if tool and #tool.fillTriggers>0 then
            tool:setIsFilling(not tool.isFilling);message=tool.isFilling and 'Refilling tool' or 'Refilling stopped'
        end
        for _,silo in ipairs(silos) do if silo:getIsActivatable() then
            silo:setIsFilling(not silo.isFilling,silo.siloTrailer.fillLevel>0 and silo.siloTrailer.currentFillType or Fillable.FILLTYPE_WHEAT)
            message=silo.isFilling and 'Filling wheat' or 'Filling stopped'
        end end
    elseif action=='tip' and v and v.implement then
        local t=v.implement
        if t.tipping then t.tipping=false;message='Unloading stopped'
        elseif t.fillLevel>0 and #t.tipReferencePoints>0 then
            local best,distance,pointIndex
            for _,trigger in ipairs(tipTriggers) do
                local allowed,d,p=trigger:getTipInfoForTrailer(t)
                if allowed and d<g_currentMission.tipTriggerRangeThreshold and (not distance or d<distance) then best,distance,pointIndex=trigger,d,p end
            end
            if best then t.currentTipTrigger=best;t.currentTipReferencePointIndex=pointIndex;t.tipTime=0;t.tipping=true;message='Unloading at '..best.stationName
            else message='Move a tipping point to a compatible unloading station' end
        end
    end
end
function missionScenario(kind)
    local v,tool,joint
    if kind=='scenario-harvest' then
        for _,candidate in ipairs(vehicles) do if candidate.hasPipe then v=candidate;break end end
        for _,candidate in ipairs(vehicles) do if candidate.jointType=='cutter' then tool=candidate;break end end
    else
        for _,candidate in ipairs(vehicles) do if candidate.fillNode and #candidate.tipReferencePoints>0 then tool=candidate;break end end
        for _,candidate in ipairs(vehicles) do if candidate.motor then for _,j in ipairs(candidate.joints) do if j.type==tool.jointType then v=candidate;break end end;if v then break end end end
    end
    assert(v and tool,'Scenario needs original vehicle and implement')
    for _,j in ipairs(v.joints) do if j.type==tool.jointType then joint=j.node;break end end
    assert(joint,'Scenario hitch type missing')
    for _,body in ipairs(vehicles) do body.speed=0 end
    if kind=='scenario-harvest' then
        local px,pz,heading
        for _,field in ipairs(g_currentMission.fields) do if field.owned and not px then
            for u=0.15,0.85,0.05 do for t=0.15,0.85,0.05 do
                local x=field[1]+u*(field[3]-field[1])+t*(field[5]-field[1])
                local z=field[2]+u*(field[4]-field[2])+t*(field[6]-field[2])
                local name,state=worldCropInfo(x,z);local desc=name and FruitUtil.fruitTypes[name]
                if desc and tool.acceptedFruits[name] and state>=desc.minHarvestingGrowthState+1 and state<=desc.maxHarvestingGrowthState+1 then
                    px,pz=x,z;local dx,dz=field[3]-field[1],field[4]-field[2];local hx,hz=field[5]-field[1],field[6]-field[2]
                    if hx*hx+hz*hz>dx*dx+dz*dz then dx,dz=hx,hz;if t>0.5 then dx,dz=-dx,-dz end elseif u>0.5 then dx,dz=-dx,-dz end
                    heading=math.atan2(dx,dz);break
                end
            end;if px then break end end
        end end
        assert(px,'No mature supported crop found in an originally owned field')
        v.x,v.z,v.yaw=px-math.sin(heading)*8,pz-math.cos(heading)*8,heading;v.y=worldHeight(v.x,v.z)+v.restHeight;pose(v)
        tool.yaw=v.yaw;pose(tool)
        local x,y,z=getWorldTranslation(joint);local tx,ty,tz=getWorldTranslation(tool.inputJoint)
        tool.x,tool.y,tool.z=tool.x+x-tx,tool.y+y-ty,tool.z+z-tz;pose(tool);tool.working=true
        message='Harvest test scenario on an original owned field'
    else
        local silo=assert(silos[1]);tool.yaw=0;pose(tool)
        local x,z=0,0
        for _,id in ipairs(silo.triggerIds) do local cx,_,cz=worldBounds(id);x,z=x+cx,z+cz end
        x,z=x/#silo.triggerIds,z/#silo.triggerIds
        local tx,_,tz=worldBounds(tool.fillNode)
        tool.x,tool.z=tool.x+x-tx,tool.z+z-tz;tool.y=worldHeight(tool.x,tool.z)+tool.restHeight;pose(tool)
        v.yaw=tool.yaw;pose(v)
        local x,_,z=getWorldTranslation(tool.inputJoint);local hx,_,hz=getWorldTranslation(joint)
        v.x,v.z=v.x+x-hx,v.z+z-hz;v.y=worldHeight(v.x,v.z)+v.restHeight;pose(v)
        message='Silo test scenario: R starts/stops filling wheat'
    end
    v.hitch=joint;v.implement=tool;tool.attacherVehicle=v
    selection=v.id;g_currentMission.controlledVehicle=v;freeCamera=false;worldSelect(selection);updateSilos(0)
end
function missionSelfcheck()
    local function check(value,name)
        assert(value,'Mission check failed: '..name)
        checks[#checks+1]=name
    end
    check(totalDistance>10,'original motor/wheel control produces travel')
    local v=g_currentMission.controlledVehicle
    for _=1,180 do missionUpdate(1/60,0,0,true) end
    check(math.abs(v.speed)<0.05,'handbrake stops vehicle')
    local heading=v.yaw
    for _=1,120 do missionUpdate(1/60,1,0.6,false) end
    check(math.abs(v.yaw-heading)>0.01,'steering changes heading')
    local t
    for _,candidate in ipairs(vehicles) do if candidate.fillNode and #candidate.tipReferencePoints>0 then t=candidate;break end end
    check(t~=nil,'original fillable trailer loaded')
    local silo=assert(silos[1])
    local old={t.x,t.y,t.z,t.yaw,t.fillLevel,t.currentFillType}
    local stock=g_currentMission:getSiloAmount(Fillable.FILLTYPE_WHEAT)
    local oldControlled=g_currentMission.controlledVehicle
    t.attacherVehicle=v;v.implement=t
    t.yaw=0;pose(t)
    local cx,cz=0,0
    for _,trigger in ipairs(silo.triggerIds) do local x,_,z=worldBounds(trigger);cx,cz=cx+x,cz+z end
    cx,cz=cx/#silo.triggerIds,cz/#silo.triggerIds
    local x,_,z=worldBounds(t.fillNode)
    t.x=t.x+cx-x;t.z=t.z+cz-z;t.y=worldHeight(t.x,t.z)+t.restHeight;pose(t)
    updateSilos(0)
    check(silo.fill==4,'four original silo trigger meshes overlap trailer (got '..silo.fill..')')
    check(silo:getIsActivatable(),'controlled attached trailer can activate silo')
    silo:setIsFilling(true,Fillable.FILLTYPE_WHEAT)
    updateSilos(1)
    check(math.abs(t.fillLevel-1500)<0.001,'original silo fills 1500 L in one second (level '..t.fillLevel..', stock '..stock..', rate '..silo.fillLitersPerSecond..')')
    check(math.abs(g_currentMission:getSiloAmount(Fillable.FILLTYPE_WHEAT)+t.fillLevel-stock)<0.001,'silo/trailer inventory conserved')
    t:setFillLevel(t.fillLevel,Fillable.FILLTYPE_BARLEY)
    check(t.currentFillType==Fillable.FILLTYPE_WHEAT,'mixed grain rejected')
    g_currentMission:setSiloAmount(Fillable.FILLTYPE_WHEAT,20000)
    t:setFillLevel(t.capacity-10,Fillable.FILLTYPE_WHEAT)
    updateSilos(1)
    check(t.fillLevel==t.capacity and g_currentMission:getSiloAmount(Fillable.FILLTYPE_WHEAT)==19990,'capacity overflow consumes only available space')
    updateSilos(1)
    check(not silo.isFilling,'full trailer stops filling')
    t:setFillLevel(0,Fillable.FILLTYPE_WHEAT,true)
    g_currentMission:setSiloAmount(Fillable.FILLTYPE_WHEAT,100)
    silo:setIsFilling(true,Fillable.FILLTYPE_WHEAT);updateSilos(1);updateSilos(1)
    check(t.fillLevel==100 and g_currentMission:getSiloAmount(Fillable.FILLTYPE_WHEAT)==0 and not silo.isFilling,'empty silo stops without inventing grain')
    g_currentMission:setSiloAmount(Fillable.FILLTYPE_WHEAT,1000)
    silo:setIsFilling(true,Fillable.FILLTYPE_WHEAT)
    t.x=t.x+20;pose(t);updateSilos(1)
    check(not silo.isFilling and silo.siloTrailer==nil,'leaving trigger stops filling')
    local farm
    for _,trigger in ipairs(tipTriggers) do if trigger.isFarmTrigger and trigger.acceptedFillTypes[Fillable.FILLTYPE_WHEAT] then farm=trigger;break end end
    check(farm~=nil,'original farm unloading trigger loaded')
    local fx,_,fz=getWorldTranslation(farm.triggerStartId)
    local px,_,pz=getWorldTranslation(t.tipReferencePoints[1].node)
    t.x=t.x+fx-px;t.z=t.z+fz-pz;t.y=worldHeight(t.x,t.z)+t.restHeight;pose(t)
    t.currentTipTrigger=farm;t.currentTipReferencePointIndex=1;t.tipTime=3000
    t:setFillLevel(1000,Fillable.FILLTYPE_WHEAT,true)
    check(farm:getTipInfoForTrailer(t,1),'unloading accepts configured grain')
    local before=g_currentMission:getSiloAmount(Fillable.FILLTYPE_WHEAT)+t.fillLevel
    Trailer.updateTipping(t,1000)
    unloaded=unloaded+1000-t.fillLevel
    check(t.fillLevel<1000,'original trailer update discharges into farm trigger')
    check(math.abs(g_currentMission:getSiloAmount(Fillable.FILLTYPE_WHEAT)+t.fillLevel-before)<0.001,'original unloading preserves farm inventory')
    check(t.fillLevel==0 and t.currentFillType==Fillable.FILLTYPE_UNKNOWN,'empty trailer clears fill type')
    t.x,t.y,t.z,t.yaw,t.fillLevel,t.currentFillType=unpack(old)
    t.currentTipTrigger=nil;t.attacherVehicle=nil;v.implement=nil;pose(t)
    g_currentMission.controlledVehicle=oldControlled;g_currentMission:setSiloAmount(Fillable.FILLTYPE_WHEAT,stock)
    updateSilos(0)
    local field
    for _,candidate in ipairs(g_currentMission.fields) do if candidate.owned then field=candidate;break end end
    check(field~=nil,'original owned field loaded')
    local x,z=(field[3]+field[5])/2,(field[4]+field[6])/2
    local id=g_currentMission.fruits[FruitUtil.FRUITTYPE_WHEAT].id
    setDensityTypeIndexCompareMode(id,2)
    setDensityParallelogram(id,x,z,4,0,0,4,0,8,6)
    setDensityTypeIndexCompareMode(id,0)
    setDensityParallelogram(1,x,z,4,0,0,4,0,4,1)
    local cut=Utils.cutFruitArea(FruitUtil.FRUITTYPE_WHEAT,x,z,x+4,z,x,z+4,true,true)
    check(cut==64,'recovered harvest cuts 64 density cells in 16 square metres')
    local name,state=worldCropInfo(x+2,z+2)
    check(name=='wheat' and state==9,'harvest writes the original cut growth state')
    check(Utils.cutFruitArea(FruitUtil.FRUITTYPE_WHEAT,x,z,x+4,z,x,z+4,true,true)==0,'harvested area cannot yield twice')
    Utils.updateCultivatorArea(x,z,x+4,z,x,z+4,false,false,0)
    check(worldCropInfo(x+2,z+2)==nil,'recovered cultivation clears crop type')
    local planted=Utils.updateSowingArea(FruitUtil.FRUITTYPE_WHEAT,x,z,x+4,z,x,z+4,0,false)
    name,state=worldCropInfo(x+2,z+2)
    check(planted==64 and name=='wheat' and state==1,'recovered sowing plants only prepared soil')
    check(Utils.updateSowingArea(FruitUtil.FRUITTYPE_WHEAT,x,z,x+4,z,x,z+4,0,false)==0,'sowing same area twice does not duplicate seed consumption')
    local combine
    for _,candidate in ipairs(vehicles) do if candidate.hasPipe then combine=candidate;break end end
    check(combine~=nil,'original combine pipe services loaded')
    local saved={combine.fillLevel,combine.currentFillType}
    combine:setFillLevel(600,Fillable.FILLTYPE_WHEAT,true)
    combine.targetPipeState,combine.currentPipeState=2,2
    modelAnimation(combine,combine.pipeAnimation,1);pose(combine)
    local px,_,pz=getWorldTranslation(combine.pipeRaycastNode)
    local tx,_,tz=worldBounds(t.fillNode)
    t.x=t.x+px-tx;t.z=t.z+pz-tz;t.y=worldHeight(t.x,t.z)+t.restHeight;pose(t)
    t:setFillLevel(0,Fillable.FILLTYPE_WHEAT,true)
    Overloading.updateTick(combine,1000)
    check(t.fillLevel==150 and combine.fillLevel==450,'recovered pipe/raycast transfers original 150 L per second')
    check(t.fillLevel+combine.fillLevel==600,'combine/trailer transfer conserves grain')
    t:setFillLevel(t.capacity-10,Fillable.FILLTYPE_WHEAT,true)
    Overloading.updateTick(combine,1000)
    check(t.fillLevel==t.capacity and combine.fillLevel==440,'combine transfer respects trailer capacity')
    t:setFillLevel(1000,Fillable.FILLTYPE_BARLEY,true)
    Overloading.updateTick(combine,1000)
    check(combine.fillLevel==440 and t.fillLevel==1000,'pipe rejects incompatible grain')
    combine.fillLevel,combine.currentFillType=unpack(saved)
    combine.targetPipeState,combine.currentPipeState=1,1;modelAnimation(combine,combine.pipeAnimation,0);pose(combine)
    t.x,t.y,t.z,t.yaw,t.fillLevel,t.currentFillType=unpack(old);pose(t)
    local seedTool,seedTrigger
    for _,candidate in ipairs(vehicles) do if candidate.fillTypes[Fillable.FILLTYPE_SEEDS] then seedTool=candidate;break end end
    for _,candidate in ipairs(fillTriggers) do if candidate.fillType==Fillable.FILLTYPE_SEEDS then seedTrigger=candidate;break end end
    check(seedTool and seedTrigger,'original seed tool and supply trigger loaded')
    local sx,sy,sz=seedTool.x,seedTool.y,seedTool.z
    local x,_,z=worldBounds(seedTrigger.triggerId);local tx,_,tz=worldBounds(seedTool.fillNode)
    seedTool.x=seedTool.x+x-tx;seedTool.z=seedTool.z+z-tz;seedTool.y=worldHeight(seedTool.x,seedTool.z)+seedTool.restHeight;pose(seedTool);updateSilos(0)
    check(#seedTool.fillTriggers>0,'native supply overlap reaches recovered fill callback')
    local cash=g_currentMission.money
    seedTool:setIsFilling(true);Fillable.updateTick(seedTool,1000)
    check(seedTool.fillLevel==100,'recovered supply uses original 100 L per second semoir configuration')
    check(math.abs(g_currentMission.money-(cash-60))<0.001,'seed refill charges original fill-type price')
    seedTool:setIsFilling(false);seedTool:setFillLevel(0,Fillable.FILLTYPE_SEEDS,true)
    seedTool.x,seedTool.y,seedTool.z=sx,sy,sz;pose(seedTool);updateSilos(0);g_currentMission.money=cash
end
function missionStatus()
    local v=g_currentMission.controlledVehicle
    local trailer=v and v.implement
    return string.format('%s | %.1f km/h | Tank %.0f/%.0f L%s\nWASD/ZQSD/arrows | TAB vehicle | E attach | B work | O pipe | R fill | I unload | F8 camera | Space brake\nWheat silo: %.0f L | Harvested %.0f L | Money %.0f | %s',
        v and v.name or 'Free camera',v and math.abs(v.speed)*3.6 or 0,
        v and v.fillLevel or 0,v and v.capacity or 0,trailer and string.format(' | %s %.0f/%.0f L',trailer.name,trailer.fillLevel,trailer.capacity) or '',g_currentMission:getSiloAmount(Fillable.FILLTYPE_WHEAT),harvested,g_currentMission.money,message)
end
function missionReport()
    return string.format('{"vehicles":%d,"silos":%d,"tip_triggers":%d,"fields":%d,"field_parallelograms":%d,"distance_m":%.6f,"silo_transfer_l":%.6f,"unloaded_l":%.6f,"harvested_l":%.6f,"worked_sqm":%.6f,"seeded_sqm":%.6f,"checks":%d,"physics":"temporary planar host; original motor and wheel control Lua"}',
        #vehicles,#silos,#tipTriggers,g_currentMission.fieldDefinitionBase and g_currentMission.fieldDefinitionBase.numberOfFields or 0,#g_currentMission.fields,totalDistance,transferred,unloaded,harvested,worked,seeded,#checks)
end
