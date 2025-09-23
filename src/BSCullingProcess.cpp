void __fastcall BSCullingProcess::Process(BSCullingProcess *this, NiAVObject *apObject)
{
  BSCullingProcess::BSCPCullingType kCullMode; // edx
  unsigned __int64 Flags; // rcx
  BSCullingProcess *v6; // rdx
  BSCompoundFrustum *pCompoundFrustum; // rax
  unsigned int m_uiActivePlanes; // esi
  int apPlaneState[258]; // [rsp+20h] [rbp-408h] BYREF

  if ( apObject->m_kWorldBound.m_iRadiusAsInt || (apObject->m_uFlags.Flags & 0x800) != 0 )
  {
    kCullMode = this->kCullMode;
    if ( (unsigned int)(kCullMode - 1) <= 1 )
    {
      if ( kCullMode == BSCP_CULL_ALLPASS )
      {
        v6 = this;
LABEL_28:
        apObject->OnVisible(apObject, v6);
        if ( this->bUpdateAccumulateFlag )
          apObject->m_uFlags.Flags |= 0x40000000000ui64;
        return;
      }
      if ( this->bUpdateAccumulateFlag )
        apObject->m_uFlags.Flags &= ~0x40000000000ui64;
    }
    else
    {
      Flags = apObject->m_uFlags.Flags;
      if ( (Flags & 0x4000000) == 0 || this->bIgnorePreprocess || kCullMode == BSCP_CULL_FORCEMULTIBOUNDSNOUPDATE )
      {
        pCompoundFrustum = this->pCompoundFrustum;
        if ( !pCompoundFrustum || !pCompoundFrustum->iFreeOp || kCullMode == BSCP_CULL_IGNOREMULTIBOUNDS )
        {
          if ( (Flags & 0x800) == 0 && this->m_kPlanes.m_uiActivePlanes )
          {
            NiCullingProcess::DoCulling(this, apObject);
            return;
          }
          goto LABEL_8;
        }
        if ( (Flags & 0x800) != 0 )
        {
LABEL_8:
          v6 = this;
          goto LABEL_28;
        }
        BSCompoundFrustum::GetActivePlaneState(this->pCompoundFrustum, apPlaneState, 0x100u);
        m_uiActivePlanes = this->m_kPlanes.m_uiActivePlanes;
        if ( (this->pCompoundFrustum->bSkipViewFrustum || this->TestBaseVisibility(this, &apObject->m_kWorldBound))
          && BSCompoundFrustum::Process(this->pCompoundFrustum, apObject) )
        {
          apObject->OnVisible(apObject, this);
          if ( this->bUpdateAccumulateFlag )
            apObject->m_uFlags.Flags |= 0x40000000000ui64;
        }
        else if ( this->bUpdateAccumulateFlag )
        {
          apObject->m_uFlags.Flags &= ~0x40000000000ui64;
        }
        BSCompoundFrustum::SetActivePlaneState(this->pCompoundFrustum, apPlaneState, 0x100u);
        this->m_kPlanes.m_uiActivePlanes = m_uiActivePlanes;
      }
      else
      {
        if ( (Flags & 0x8000000000i64) == 0 )
          goto LABEL_8;
        if ( this->bUpdateAccumulateFlag )
          apObject->m_uFlags.Flags = Flags & 0xFFFFFBFFFFFFFFFFui64;
      }
    }
  }
}

void __fastcall BSCullingProcess::Process(
        BSCullingProcess *this,
        const NiCamera *apCamera,
        NiAVObject *apScene,
        NiVisibleArray *apVisibleSet)
{
  NiAccumulator *v8; // rbx
  NiVisibleArray *m_pkVisibleSet; // r14
  NiAccumulator *m_pObject; // rax

  if ( apCamera && apScene )
  {
    this->m_pkCamera = apCamera;
    NiCullingProcess::SetFrustum(this, &apCamera->m_kViewFrustum);
    if ( this->bCustomCullPlanes )
      NiFrustumPlanes::operator=(&this->m_kPlanes, &this->kCustomCullPlanes);
    v8 = 0i64;
    m_pkVisibleSet = 0i64;
    if ( apVisibleSet )
    {
      m_pkVisibleSet = this->m_pkVisibleSet;
      this->m_pkVisibleSet = apVisibleSet;
    }
    if ( !this->m_pkVisibleSet )
    {
      m_pObject = this->spAccumulator.m_pObject;
      if ( m_pObject )
      {
        v8 = this->spAccumulator.m_pObject;
        _InterlockedIncrement((volatile signed __int32 *)&m_pObject->m_uiRefCount);
        m_pObject->StartAccumulating(v8, apCamera);
      }
    }
    NiAVObject::Cull(apScene, this);
    if ( apVisibleSet )
      this->m_pkVisibleSet = m_pkVisibleSet;
    if ( v8 )
    {
      if ( !_InterlockedDecrement((volatile signed __int32 *)&v8->m_uiRefCount) )
        v8->DeleteThis(v8);
    }
  }
}

void __fastcall BSCullingProcess::ProcessNonRecurse(BSCullingProcess *this, NiAVObject *apObject)
{
  BSCullingProcess *v3; // rbx
  BSCullingProcess::BSCPCullingType kCullMode; // edx
  unsigned __int64 Flags; // rcx
  BSCullingProcess_vtbl *v6; // rax
  BSCompoundFrustum *pCompoundFrustum; // rax
  unsigned int m_uiActivePlanes; // esi
  int apPlaneState[258]; // [rsp+20h] [rbp-408h] BYREF

  v3 = this;
  if ( !apObject->m_kWorldBound.m_iRadiusAsInt && (apObject->m_uFlags.Flags & 0x800) == 0 )
    return;
  kCullMode = this->kCullMode;
  if ( (unsigned int)(kCullMode - 1) <= 1 )
  {
    if ( kCullMode != BSCP_CULL_ALLPASS )
      return;
    v6 = this->__vftable;
    goto LABEL_24;
  }
  Flags = apObject->m_uFlags.Flags;
  if ( (Flags & 0x4000000) != 0 && !v3->bIgnorePreprocess && kCullMode != BSCP_CULL_FORCEMULTIBOUNDSNOUPDATE )
  {
    if ( (Flags & 0x8000000000i64) != 0 )
      return;
    goto LABEL_8;
  }
  pCompoundFrustum = v3->pCompoundFrustum;
  if ( !pCompoundFrustum || !pCompoundFrustum->iFreeOp || kCullMode == BSCP_CULL_IGNOREMULTIBOUNDS )
  {
    if ( (Flags & 0x800) == 0 && v3->m_kPlanes.m_uiActivePlanes )
    {
      if ( !v3->TestBaseVisibility(v3, &apObject->m_kWorldBound) )
        return;
      v6 = v3->__vftable;
      this = v3;
      goto LABEL_24;
    }
LABEL_8:
    v6 = v3->__vftable;
    this = v3;
LABEL_24:
    v6->AppendNonAccum(this, apObject);
    return;
  }
  if ( (Flags & 0x800) != 0 )
    goto LABEL_8;
  BSCompoundFrustum::GetActivePlaneState(v3->pCompoundFrustum, apPlaneState, 0x100u);
  m_uiActivePlanes = v3->m_kPlanes.m_uiActivePlanes;
  if ( (v3->pCompoundFrustum->bSkipViewFrustum || v3->TestBaseVisibility(v3, &apObject->m_kWorldBound))
    && BSCompoundFrustum::Process(v3->pCompoundFrustum, apObject) )
  {
    v3->AppendNonAccum(v3, apObject);
  }
  BSCompoundFrustum::SetActivePlaneState(v3->pCompoundFrustum, apPlaneState, 0x100u);
  v3->m_kPlanes.m_uiActivePlanes = m_uiActivePlanes;
}

void __fastcall BSCullingProcess::ProcessNonRecurse(
        BSCullingProcess *this,
        const NiCamera *apCamera,
        NiAVObject *apScene)
{
  NiAccumulator *m_pObject; // rbx

  if ( apCamera && apScene )
  {
    this->m_pkCamera = apCamera;
    NiCullingProcess::SetFrustum(this, &apCamera->m_kViewFrustum);
    if ( this->bCustomCullPlanes )
      NiFrustumPlanes::operator=(&this->m_kPlanes, &this->kCustomCullPlanes);
    m_pObject = this->spAccumulator.m_pObject;
    if ( m_pObject )
    {
      _InterlockedIncrement((volatile signed __int32 *)&m_pObject->m_uiRefCount);
      m_pObject->StartAccumulating(m_pObject, apCamera);
    }
    BSCullingProcess::ProcessNonRecurse(this, apScene);
    if ( m_pObject )
    {
      if ( !_InterlockedDecrement((volatile signed __int32 *)&m_pObject->m_uiRefCount) )
        m_pObject->DeleteThis(m_pObject);
    }
  }
}