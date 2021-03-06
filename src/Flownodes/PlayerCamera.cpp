/* Camera_Plugin - for licensing and copyright see license.txt */

#include <StdAfx.h>
#include <CPluginCamera.h>

#include <Nodes/G2FlowBaseNode.h>

#include <Game.h>
#include <Cry_Camera.h>
#include <Camera/CameraCommon.h>
#include <Actor.h>
#include <IViewSystem.h>
#include <Single.h>

namespace CameraPlugin
{
    class CFlowPlayerCameraNode :
        public CFlowBaseNode<eNCT_Instanced>
    {
        private:

            enum EInputPorts
            {
                EIP_ACTIVATE = 0,
                EIP_COLLSION,
                EIP_3RDPERSON,
                EIP_FOV,
                EIP_MINP,
                EIP_MINPOFFSETPOS,
                EIP_MAXP,
                EIP_MAXPOFFSETPOS,
                EIP_TOFFSETPOS,
                //  EIP_TOFFSETROT,
                EIP_COFFSETPOS,
                EIP_COFFSETROT,

                EIP_APOS,
                EIP_APOSBONE,

                EIP_AROT,
                EIP_AROTBONE,
                EIP_AIMFIX_EXPERIMENT,
            };

            enum EAnchorType
            {
                ETYPE_ENTITY = 1,
                ETYPE_VIEW,
                ETYPE_BONE,
            };

            IEntity* m_pEntity;
            IEntity* m_pCameraEnt;
            IView*   m_pCameraView;
        public:

            CFlowPlayerCameraNode( SActivationInfo* pActInfo )
            {
                m_pEntity       = NULL;
                m_pCameraEnt    = NULL;
                m_pCameraView   = NULL;
            }

            virtual void GetMemoryUsage( ICrySizer* s ) const
            {
                s->Add( *this );
            }

            virtual IFlowNodePtr Clone( SActivationInfo* pActInfo )
            {
                return new CFlowPlayerCameraNode( pActInfo );
            }

            virtual ~CFlowPlayerCameraNode()
            {

            }

            virtual void GetConfiguration( SFlowNodeConfig& config )
            {
                static const SInputPortConfig inputs[] =
                {
                    InputPortConfig_Void( "Activate",                                    _HELP( "Activate" ) ),

                    InputPortConfig<bool>( "collision",              true,               _HELP( "WIP Enables collision detection" ),       "WIP collision" ),
                    InputPortConfig<bool>( "thirdperson",            true,               _HELP( "WIP Enables 3rd person mode" ),           "WIP thirdperson" ),

                    InputPortConfig<float>( "FOV",                   75,                 _HELP( "Field of View [DEG]" ) ),

                    InputPortConfig<float>( "MinPitch",              -90,                _HELP( "Min Pitch" ) ),
                    InputPortConfig<Vec3>( "MinPitchOffsetPosition", Vec3( ZERO ),         _HELP( "Min Pitch Position Offset (local XYZ)" ), "MinPitch Position Offset" ),
                    InputPortConfig<float>( "MaxPitch",              90,                 _HELP( "Max Pitch" ) ),
                    InputPortConfig<Vec3>( "MaxPitchOffsetPosition", Vec3( ZERO ),         _HELP( "Max Pitch Position Offset (local XYZ)" ), "MaxPitch Position Offset" ),

                    InputPortConfig<Vec3>( "TargetOffsetPosition",   Vec3( 0.5, -2, 1.5 ), _HELP( "Target Position Offset (local XYZ)" ),    "Target Offset Position" ),
                    //InputPortConfig<Vec3>("TargetOffsetRotation", Vec3(ZERO),         _HELP("Target Position Offset (YPR [DEG])"),    "WIP Target Offset Rotation"),
                    InputPortConfig<Vec3>( "CameraOffsetPosition",   Vec3( ZERO ),         _HELP( "Camera Position Offset (local XYZ)\r\nA Sphere around the Camera Target will be created with this offset as the origin." ),   "Camera Offset Position" ),
                    InputPortConfig<Vec3>( "CameraOffsetRotation",   Vec3( ZERO ),         _HELP( "Camera Rotation Offset (YPR [DEG])" ),    "Camera Rotation Offset" ),

                    InputPortConfig<int>( "PositionAnchor",          ETYPE_ENTITY,       _HELP( "Anchor Position" ),                       "Position Anchor",              _UICONFIG( "enum_int:Entity=1,View=2,Bone=3" ) ),
                    InputPortConfig<string>( "bone_PositionAnchor",  "",                 _HELP( "Position Anchor Bone" ),                  "Position Anchor Bone",         _UICONFIG( "ref_entity=entityId" ) ),
                    InputPortConfig<int>( "RotationAnchor",          ETYPE_VIEW,         _HELP( "Anchor Rotation" ),                       "Rotation Anchor",              _UICONFIG( "enum_int:Entity=1,View=2,Bone=3" ) ),
                    InputPortConfig<string>( "bone_RotationAnchor",  "",                 _HELP( "Rotation Anchor Bone" ),                  "Rotation Anchor Bone",         _UICONFIG( "ref_entity=entityId" ) ),
                    InputPortConfig<bool>( "aimfix",              false,               _HELP( "Experimental Aim correction" ),       "Experimental Aim correction" ),
                    InputPortConfig_Null(),
                };

                config.pInputPorts = inputs;
                config.pOutputPorts = NULL;
                config.sDescription = _HELP( "Control anchored camera." );

                config.nFlags |= EFLN_TARGET_ENTITY;
                config.SetCategory( EFLN_APPROVED );
            }

            virtual void ProcessEvent( EFlowEvent evt, SActivationInfo* pActInfo )
            {
                switch ( evt )
                {
                    case eFE_Suspend:
                        pActInfo->pGraph->SetRegularlyUpdated( pActInfo->myID, false );
                        break;

                    case eFE_Resume:
                        pActInfo->pGraph->SetRegularlyUpdated( pActInfo->myID, true );
                        break;

                    case eFE_Activate:
                        if ( IsPortActive( pActInfo, EIP_ACTIVATE ) )
                        {
                            pActInfo->pGraph->SetRegularlyUpdated( pActInfo->myID, true );

                            m_pCameraEnt = gEnv->pEntitySystem->FindEntityByName( "PlayerCamera" );

                            if ( !m_pCameraEnt )
                            {
                                // Create the camera entity ... construct Spawn Parameters
                                SEntitySpawnParams spawn;

                                spawn.sName = "PlayerCamera";
                                spawn.pClass = gEnv->pEntitySystem->GetClassRegistry()->GetDefaultClass();
                                spawn.vPosition = gEnv->pSystem->GetViewCamera().GetPosition();
                                spawn.qRotation = gEnv->pEntitySystem->GetEntity( gEnv->pGameFramework->GetClientActorId() )->GetRotation();
                                m_pCameraEnt = gEnv->pEntitySystem->SpawnEntity( spawn );

                                m_pCameraView = gEnv->pGameFramework->GetIViewSystem()->GetViewByEntityId( m_pCameraEnt->GetId(), true );

                                if ( m_pCameraView )
                                {
                                    SViewParams sViewPar = *( m_pCameraView->GetCurrentParams() );
                                    sViewPar.fov = DEG2RAD( 75 );
                                    m_pCameraView->SetCurrentParams( sViewPar );
                                    gEnv->pGameFramework->GetIViewSystem()->SetActiveView( m_pCameraView );
                                }
                            }
                        }

                        break;

                    case eFE_SetEntityId:
                        m_pEntity = pActInfo->pEntity;

                        break;

                    case eFE_Update:
                        if ( m_pCameraEnt )
                        {
                            // Get Actor
                            IActor* pActor = NULL;

                            if ( m_pEntity )
                            {
                                pActor = gEnv->pGameFramework->GetIActorSystem()->GetActor( m_pEntity->GetId() );
                            }

                            else
                            {
                                pActor = gEnv->pGameFramework->GetClientActor();
                                m_pEntity = pActor->GetEntity();
                            }

                            // Get Animated Character
                            IAnimatedCharacter* pAnimCharacter = NULL;

                            if ( pActor )
                            {
                                pAnimCharacter = pActor->GetAnimatedCharacter();
                            }

                            // Get Character Instance
                            ICharacterInstance* pCharacter = NULL;

                            if ( pActor )
                            {
                                pCharacter = pActor->GetEntity()->GetCharacter( 0 );
                            }

                            // Get Skeleton Pose
                            ISkeletonPose*  pPose = NULL;

                            if ( pCharacter )
                            {
                                pPose = pCharacter->GetISkeletonPose();
                            }

                            // Get View
                            const SViewParams* sViewPar = NULL;

                            if ( m_pEntity )
                            {
                                IViewSystem* pViewSystem = gEnv->pGameFramework->GetIViewSystem();

                                IView* pView = NULL;

                                if ( pViewSystem )
                                {
                                    pView = pViewSystem->GetViewByEntityId( m_pEntity->GetId() );
                                }

                                if ( pView )
                                {
                                    pView->Update( gEnv->pTimer->GetFrameTime(), true ); // Force a CPlayer ViewUpdate since otherwise the view isn't updated
                                    sViewPar = pView->GetCurrentParams();
                                }
                            }

#define CHECK_POINTER(ptr) assert(ptr);if(!ptr)return;

                            Vec3 vecCamPos;

                            switch ( GetPortInt( pActInfo, EIP_APOS ) )
                            {
                                case ETYPE_ENTITY:
                                    CHECK_POINTER( m_pEntity );
                                    vecCamPos = m_pEntity->GetPos();
                                    break;

                                case ETYPE_VIEW:
                                    CHECK_POINTER( sViewPar );
                                    vecCamPos = sViewPar->position;
                                    break;

                                case ETYPE_BONE:
                                    CHECK_POINTER( pAnimCharacter );
                                    CHECK_POINTER( pPose );

                                    int16 nBoneid = pPose->GetJointIDByName( GetPortString( pActInfo, EIP_APOSBONE ) );

                                    if ( nBoneid >= 0 )
                                    {
                                        QuatT localBone         = pPose->GetAbsJointByID( nBoneid );
                                        QuatT worldparentBone   = pAnimCharacter->GetAnimLocation();
                                        QuatT worldBone         = worldparentBone * localBone;
                                        vecCamPos = worldBone.t;
                                    }

                                    else
                                    {
                                        vecCamPos = Vec3( 0, 0, 0 );
                                    }

                                    break;
                            }

                            Quat vecCamRot;

                            switch ( GetPortInt( pActInfo, EIP_AROT ) )
                            {
                                case ETYPE_ENTITY:
                                    CHECK_POINTER( m_pEntity );
                                    vecCamRot = m_pEntity->GetRotation();
                                    break;

                                case ETYPE_VIEW:
                                    CHECK_POINTER( sViewPar );
                                    vecCamRot = sViewPar->rotation;
                                    //vecCamRot = pActor->GetViewRotation();
                                    break;

                                case ETYPE_BONE:
                                    CHECK_POINTER( pAnimCharacter );
                                    CHECK_POINTER( pPose );
                                    int16 nBoneid = pPose->GetJointIDByName( GetPortString( pActInfo, EIP_AROTBONE ) );

                                    if ( nBoneid >= 0 )
                                    {
                                        QuatT localBone         = pPose->GetAbsJointByID( nBoneid );
                                        QuatT worldparentBone   = pAnimCharacter->GetAnimLocation();
                                        QuatT worldBone         = worldparentBone * localBone;
                                        vecCamRot = worldBone.q;
                                    }

                                    else
                                    {
                                        vecCamRot.SetIdentity();
                                    }

                                    break;
                            }

                            //SIKLimb *pLimb = m_pActor->GetIKLimb(m_grabStats.limbId[0]);
                            //ICharacterInstance *pCharacter = pEnt->GetCharacter(pLimb->characterSlot);

                            // Calculate Target offset position
                            Vec3 vecCamPos_offset = GetPortVec3( pActInfo, EIP_TOFFSETPOS );

                            // Calculate Camera offset rotation
                            Matrix33 vecCamRot_matrix( vecCamRot );
                            Ang3 temp = CCamera::CreateAnglesYPR( vecCamRot_matrix );

                            Ang3 vecCamRot_offset = Ang3( GetPortVec3( pActInfo, EIP_COFFSETROT ) );
                            vecCamRot_offset.x = DEG2RAD( vecCamRot_offset.x );
                            vecCamRot_offset.y = DEG2RAD( vecCamRot_offset.y );
                            vecCamRot_offset.z = DEG2RAD( vecCamRot_offset.z );

                            // Decide if a Sphere should be created
                            if ( GetPortVec3( pActInfo, EIP_COFFSETPOS ).len() > g_fCamError )
                            {
                                // Use Spherical Camera
                                SSpherical  local_sphereCamTargetOrigin;
                                SSpherical  local_sphereCamTarget;
                                Vec3        local_sphereCartesian = GetPortVec3( pActInfo, EIP_COFFSETPOS );

                                CartesianToSpherical(
                                    local_sphereCartesian,
                                    local_sphereCamTargetOrigin.m_fYaw,
                                    local_sphereCamTargetOrigin.m_fPitch,
                                    local_sphereCamTargetOrigin.m_fDist );

                                local_sphereCamTarget = local_sphereCamTargetOrigin;

                                // Apply Rotation
                                float fAltitude = CLAMP( temp.y, DEG2RAD( GetPortFloat( pActInfo, EIP_MINP ) ), DEG2RAD( GetPortFloat( pActInfo, EIP_MAXP ) ) ); // Using Pitch Limit
                                local_sphereCamTarget.m_fPitch  += fAltitude;
                                //local_sphereCamTarget.m_fYaw  += temp.x; // TODO: if needed then additional input reqiured

                                // Handle not supported Angles
                                if ( abs( local_sphereCamTarget.m_fPitch ) < g_fCamError )
                                {
                                    local_sphereCamTarget.m_fPitch = -g_fCamError;
                                }

                                // Apply to position
                                Vec3 vecCamPos_offsetSphere = SphericalToCartesian( local_sphereCamTarget );

                                // Apply Scaled MinMax Pitch Offset
                                float fAltitudeScale;

                                if ( fAltitude < 0 )
                                {
                                    fAltitudeScale          = fAltitude / DEG2RAD( GetPortFloat( pActInfo, EIP_MINP ) );
                                    vecCamPos_offsetSphere  += GetPortVec3( pActInfo, EIP_MINPOFFSETPOS ) * fAltitudeScale;
                                }

                                else
                                {
                                    fAltitudeScale          = fAltitude / DEG2RAD( GetPortFloat( pActInfo, EIP_MAXP ) );
                                    vecCamPos_offsetSphere  += GetPortVec3( pActInfo, EIP_MAXPOFFSETPOS ) * fAltitudeScale;
                                }

                                // Look to camera target
                                Vec3 lookAt = -pActor->GetEntity()->GetLocalTM().TransformVector( vecCamPos_offsetSphere );

                                // Apply Sphere Offset
                                vecCamPos_offset += vecCamPos_offsetSphere;

                                // Apply Camera Offset Rotation
                                if ( vecCamPos_offsetSphere.y > 0 ) // TODO: Create orientation using up forward right vecs using getorthogonal + rotate
                                {
                                    vecCamRot_matrix = Matrix33::CreateRotationVDir( lookAt.GetNormalized(), DEG2RAD( 180 ) );
                                }

                                else
                                {
                                    vecCamRot_matrix = Matrix33::CreateRotationVDir( lookAt.GetNormalized() );
                                }

                                temp = CCamera::CreateAnglesYPR( vecCamRot_matrix );
                                temp += vecCamRot_offset;
                            }

                            else
                            {
                                // Use Rotation Anchor Camera

                                // Apply Camera Offset Rotation
                                temp += vecCamRot_offset;
                                temp.y = CLAMP( temp.y, DEG2RAD( GetPortFloat( pActInfo, EIP_MINP ) ), DEG2RAD( GetPortFloat( pActInfo, EIP_MAXP ) ) ); // Apply Pitch Limit

                                // Apply Scaled MinMax Pitch Offset
                                float fPitchScale;

                                if ( temp.y < 0 )
                                {
                                    fPitchScale         = temp.y / DEG2RAD( GetPortFloat( pActInfo, EIP_MINP ) );
                                    vecCamPos_offset    += GetPortVec3( pActInfo, EIP_MINPOFFSETPOS ) * fPitchScale;
                                }

                                else
                                {
                                    fPitchScale         = temp.y / DEG2RAD( GetPortFloat( pActInfo, EIP_MAXP ) );
                                    vecCamPos_offset    += GetPortVec3( pActInfo, EIP_MAXPOFFSETPOS ) * fPitchScale;
                                }
                            }

                            // Apply transformed local offset
                            // (relative yaw,pitch translation) -> warscheinlich nur f�r target offset
                            vecCamPos += Matrix34( pActor->GetViewRotation() ).TransformVector( vecCamPos_offset );
                            // (relative yaw translation)
                            //vecCamPos += pActor->GetEntity()->GetLocalTM().TransformVector(vecCamPos_offset);

                            // Create Rotation
                            vecCamRot_matrix = CCamera::CreateOrientationYPR( temp );
                            vecCamRot = Quat( vecCamRot_matrix );

                            // Now Set info in camera entity
                            if ( m_pCameraEnt && m_pCameraView && gEnv && gEnv->pEntitySystem && gEnv->pEntitySystem->FindEntityByName( "PlayerCamera" ) )
                            {
                                {
                                    SViewParams sViewPar = *( m_pCameraView->GetCurrentParams() );
                                    sViewPar.fov = DEG2RAD( GetPortFloat( pActInfo, EIP_FOV ) );
                                    m_pCameraView->SetCurrentParams( sViewPar );
                                }

                                m_pCameraEnt->SetPos( vecCamPos );

                                // (calculated rotation) -> TODO currently throw it out
                                vecCamRot = sViewPar->rotation;
                                m_pCameraEnt->SetRotation( vecCamRot );

                                // Aimfix experiment
                                if ( GetPortBool( pActInfo, EIP_AIMFIX_EXPERIMENT ) )
                                {
                                    const int objects = ent_all;
                                    const int flags = ( geom_colltype_ray << rwi_colltype_bit ) | rwi_colltype_any | ( 10 & rwi_pierceability_mask ) | ( geom_colltype14 << rwi_colltype_bit );

                                    IItem* pItem = pActor->GetCurrentItem();
                                    IWeapon* pWeapon = pItem ? pItem->GetIWeapon() : NULL;

                                    IPhysicalEntity* pSkipEntities[11];
                                    int nSkip = 0;

                                    if ( pWeapon )
                                    {
                                        // CWeapon
                                        //nSkip = pWeapon-> GetSkipEntities( pWeapon, pSkipEntities, 10 );
                                    }

                                    pSkipEntities[nSkip++] = pActor->GetEntity()->GetPhysics();

                                    Vec3 posweapon = sViewPar->position;
                                    Vec3 dirweapon = sViewPar->rotation.GetColumn1();

                                    if ( pWeapon )
                                    {
                                        Vec3 probhit = Vec3( 0, 0, 0 );
                                        posweapon = pWeapon->GetFiringPos( probhit );
                                        dirweapon = pWeapon->GetFiringDir( posweapon, probhit );
                                    }

                                    dirweapon.Normalize();

                                    ray_hit rayhit;

                                    if ( gEnv->pPhysicalWorld->RayWorldIntersection( posweapon, dirweapon * WEAPON_HIT_RANGE, objects, flags, &rayhit, 1, pSkipEntities, nSkip ) )
                                    {
                                        gEnv->pRenderer->GetIRenderAuxGeom()->DrawSphere( posweapon, 0.1f, ColorB( 255, 128, 0 ) );
                                        gEnv->pRenderer->GetIRenderAuxGeom()->DrawLine( posweapon, ColorB( 128, 0, 0 ), rayhit.pt, ColorB( 128, 0, 0 ), 6.0f );

                                        Vec3 testv = rayhit.pt - vecCamPos;
                                        testv.Normalize();
                                        Quat testr = Quat::CreateRotationVDir( testv, 0 );
                                        m_pCameraEnt->SetRotation( testr );
                                    }
                                }
                            }

                            else
                            {
                                m_pCameraEnt = NULL;
                                m_pCameraView = NULL;
                            }
                        }

                        break;
                }
            }
    };
}

REGISTER_FLOW_NODE_EX( "Plugin_Camera:PlayerCamera", CameraPlugin::CFlowPlayerCameraNode, CFlowPlayerCameraNode );