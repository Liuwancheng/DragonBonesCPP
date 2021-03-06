#include "Armature.h"
#include "Bone.h"
#include "Slot.h"
#include "../animation/Animation.h"
#include "../events/EventObject.h"

DRAGONBONES_NAMESPACE_BEGIN

IEventDispatcher* Armature::soundEventManager = nullptr;

Armature::Armature() :
    _animation(nullptr),
    _display(nullptr)
{
    _onClear();
}
Armature::~Armature()
{
    _onClear();
}

void Armature::_onClear()
{
    userData = nullptr;

    _bonesDirty = false;
    _cacheFrameIndex = -1;
    _armatureData = nullptr;
    _skinData = nullptr;

    if (_animation)
    {
        _animation->returnToPool();
        _animation = nullptr;
    }

    if (_display)
    {
        _display->_onClear();
        _display = nullptr;
    }

    _replaceTexture = nullptr;
    _parent = nullptr;
    _action = nullptr;

    _delayDispose = false;
    _lockDispose = false;
    _lockEvent = false;
    _slotsDirty = false;
}

void Armature::_sortBones()
{
    if (_bones.empty())
    {
        return;
    }

    const auto total = _bones.size();
    const auto sortHelper = _bones;
    std::size_t index = 0;
    std::size_t count = 0;

    _bones.clear();
    _bones.resize(total, nullptr);

    while (count < total)
    {
        const auto bone = sortHelper[index++];

        if (index >= total)
        {
            index = 0;
        }

        if (std::find(_bones.cbegin(), _bones.cend(), bone) != _bones.cend())
        {
            continue;
        }

        if (bone->getParent() && std::find(_bones.cbegin(), _bones.cend(), bone->getParent()) == _bones.cend())
        {
            continue;
        }

        if (bone->getIK() && std::find(_bones.cbegin(), _bones.cend(), bone->getIK()) == _bones.cend())
        {
            continue;
        }

        if (bone->getIK() && bone->getIKChain() > 0 && bone->getIKChainIndex() == bone->getIKChain())
        {
            auto parentInerator = std::find(_bones.begin(), _bones.end(), bone->getParent());
            _bones.insert(parentInerator + 1, bone);
            count++;
        }
        else
        {
            _bones[count++] = bone;
        }
    }

    _bones.resize(total);
}

void Armature::_sortSlots()
{
}

void Armature::_addBoneToBoneList(Bone* value)
{
    if (std::find(_bones.begin(), _bones.end(), value) == _bones.end())
    {
        _bonesDirty = true;
        _bones.push_back(value);
        _animation->_timelineStateDirty = true;
    }
}

void Armature::_removeBoneFromBoneList(Bone* value)
{
    const auto iterator = std::find(_bones.begin(), _bones.end(), value);
    if (iterator != _bones.end())
    {
        _bones.erase(iterator);
        _animation->_timelineStateDirty = true;
    }
}

void Armature::_addSlotToSlotList(Slot* value)
{
    if (std::find(_slots.begin(), _slots.end(), value) == _slots.end())
    {
        _slotsDirty = true;
        _slots.push_back(value);
        _animation->_timelineStateDirty = true;
    }
}

void Armature::_removeSlotFromSlotList(Slot* value)
{
    const auto iterator = std::find(_slots.begin(), _slots.end(), value);
    if (iterator != _slots.end())
    {
        _slots.erase(iterator);
        _animation->_timelineStateDirty = true;
    }
}

void Armature::_bufferEvent(EventObject* value, const std::string& type)
{
    value->type = type;
    value->armature = this;
    _events.push_back(value);
}

void Armature::dispose()
{
    _delayDispose = true;

    if (!_lockDispose)
    {
        this->returnToPool();
    }
}

void Armature::advanceTime(float passedTime)
{
    _lockDispose = true;

    const auto scaledPassedTime = passedTime * _animation->timeScale;

    //
    _animation->_advanceTime(passedTime);

    //
    if (_bonesDirty)
    {
        _bonesDirty = false;
        _sortBones();
    }

    if (_slotsDirty)
    {
        _slotsDirty = false;
        _sortSlots();
    }

    //
    for (const auto bone : _bones)
    {
        bone->_update(_cacheFrameIndex);
    }

    for (const auto slot : _slots)
    {
        slot->_update(_cacheFrameIndex);

        const auto childArmature = slot->getChildArmature();
        if (childArmature)
        {
            if (slot->inheritAnimation)
            {
                childArmature->advanceTime(scaledPassedTime);
            }
            else
            {
                childArmature->advanceTime(passedTime);
            }
        }
    }

    if (!_lockEvent && !_events.empty())
    {
        _lockEvent = true;

        for (const auto event : _events)
        {
            if (Armature::soundEventManager && event->type == EventObject::SOUND_EVENT)
            {
                Armature::soundEventManager->_dispatchEvent(event);
            }
            else
            {
                _display->_dispatchEvent(event);
            }

            event->returnToPool();
        }

        _events.clear();

        _lockEvent = false;
    }

    if (_action)
    {
        const auto& ints = std::get<0>(_action->data);
        const auto& floats = std::get<1>(_action->data);
        const auto& strings = std::get<2>(_action->data);

        switch (_action->type)
        {
            case ActionType::Play:
                _animation->play(strings[0], ints[0]);
                break;

            case ActionType::Stop:
                _animation->stop(strings[0]);
                break;

            case ActionType::GotoAndPlay:
                _animation->gotoAndPlayByTime(strings[0], floats[0], ints[0]);
                break;

            case ActionType::GotoAndStop:
                _animation->gotoAndStopByTime(strings[0], floats[0]);
                break;

            case ActionType::FadeIn:
                _animation->fadeIn(strings[0], floats[0], ints[0]);
                break;

            case ActionType::FadeOut:
                // TODO
                break;
        }

        _action = nullptr;
    }

    _lockDispose = false;

    if (_delayDispose)
    {
        this->returnToPool();
    }
}

void Armature::invalidUpdate(const std::string& boneName, bool updateSlotDisplay)
{
    if (boneName.empty())
    {
        for (const auto bone: _bones)
        {
            bone->invalidUpdate();
        }

        if (updateSlotDisplay)
        {
            for (const auto slot : _slots)
            {
                slot->invalidUpdate();
            }
        }
    }
    else
    {
        const auto bone = getBone(boneName);
        if (bone)
        {
            bone->invalidUpdate();

            if (updateSlotDisplay)
            {
                for (const auto slot : _slots)
                {
                    if (slot->getParent() == bone)
                    {
                        slot->invalidUpdate();
                    }
                }
            }
        }
    }
}

Slot* Armature::getSlot(const std::string& name) const
{
    for (const auto slot : _slots)
    {
        if (slot->name == name)
        {
            return slot;
        }
    }

    return nullptr;
}

Slot* Armature::getSlotByDisplay(void* display) const
{
    for (const auto slot : _slots)
    {
        if (slot->getDisplay() == display)
        {
            return slot;
        }
    }

    return nullptr;
}

void Armature::addSlot(Slot* value, const std::string& boneName)
{
    const auto bone = getBone(boneName);
    if (bone)
    {
        value->_setArmature(this);
        value->_setParent(bone);
    }
    else
    {
        DRAGONBONES_ASSERT(false, "Argument error.");
    }
}

void Armature::removeSlot(Slot* value)
{
    if (value && value->getArmature() == this)
    {
        value->_setParent(nullptr);
        value->_setArmature(nullptr);
    }
    else
    {
        DRAGONBONES_ASSERT(false, "Argument error.");
    }
}

Bone* Armature::getBone(const std::string& name) const
{
    for (const auto bone : _bones)
    {
        if (bone->name == name)
        {
            return bone;
        }
    }

    return nullptr;
}

Bone* Armature::getBoneByDisplay(void* display) const
{
    const auto slot = getSlotByDisplay(display);

    return slot ? slot->getParent() : nullptr;
}

void Armature::addBone(Bone* value, const std::string& parentName)
{
    if (value)
    {
        value->_setArmature(this);
        value->_setParent(parentName.empty() ? nullptr : getBone(parentName));
    }
    else
    {
        DRAGONBONES_ASSERT(false, "Argument error.");
    }
}

void Armature::removeBone(Bone* value)
{
    if (value && value->getArmature() == this)
    {
        value->_setParent(nullptr);
        value->_setArmature(nullptr);
    }
    else
    {
        DRAGONBONES_ASSERT(false, "Argument error.");
    }
}

void Armature::setCacheFrameRate(unsigned value)
{
    if (_armatureData->cacheFrameRate == value)
    {
        return;
    }

    _armatureData->cacheFrames(value);
}

DRAGONBONES_NAMESPACE_END
