#ifndef __HELLO_DRAGONBONES_H__
#define __HELLO_DRAGONBONES_H__

#include "cocos2d.h"
#include "cocos2dx/CCDragonBonesHeaders.h"

class HelloDragonBones : public cocos2d::Layer
{
public:
    static cocos2d::Scene* createScene();

    virtual bool init();

    CREATE_FUNC(HelloDragonBones);

private:
    dragonBones::Armature* _armature;
    dragonBones::CCArmatureDisplayContainer* _armatureDisplay;
    dragonBones::CCFactory _factory;

    bool _touchHandler(const cocos2d::Touch* touch, cocos2d::Event* event) const;
};

#endif // __HELLO_DRAGONBONES_H__
