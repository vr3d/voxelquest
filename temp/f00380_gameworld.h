// f00380_gameworld.h
//

#include "f00380_gameworld.e"
#define LZZ_INLINE inline
GameWorld::GameWorld ()
        {

		// Notes
		// -----
		// Depth: 16 bits
		// Material: 16 bits
		// AO val: 8 bits, normal: 24 bits

	}
bool GameWorld::checkBounds (int i, int j, int k)
                                              {
		
		bool res = true;

		if (i < 0) {res = false;}
		if (j < 0) {res = false;}
		if (k < 0) {res = false;}
		if (i >= worldSizeInPages.getIX()) {res = false;}
		if (j >= worldSizeInPages.getIY()) {res = false;}
		if (k >= worldSizeInPages.getIZ()) {res = false;}

		return res;
	}
void GameWorld::resetToState (E_STATES resState)
                                             {
		int i;

		threadpool.joinAll();

		for (i = 0; i < iVolumeSize; i++) {
			if (worldData[i]) {
				if( worldData[i]->curState > resState) {

					if (worldData[i]->fillState == E_FILL_STATE_PARTIAL) {
						worldData[i]->curState = resState;
					}

					
				}
			}
		}
	}
void GameWorld::init (Singleton * _singleton)
                                         {

		#ifdef DEBUG_MODE
		pushTrace("GameWorld init");
		#endif

		int i;
		int j;

		pageCount = 0;
		lastProcResult = true;
		maxThreads = 7;
		availThreads = maxThreads;

		for (i = 0; i < maxThreads; i++) {
			ocThreads.push_back(-1);
		}

		

		

		doDrawFBO = false;

		for (j = 0; j < E_RENDER_LENGTH; j++) {
			for (i = 0; i < E_STATE_LENGTH; i++) {
				diagrams[j][i] = E_STATE_WAIT;
			}
		}

		renderMethod = (int)E_RENDER_VOL;
		singleton = _singleton;
		worldSizeInPages.copyFrom( &(singleton->worldSizeInPages) );

		visPageSizeInPixels = singleton->visPageSizeInPixels;

	    diagrams[E_RENDER_VOL][E_STATE_INIT_END] = E_STATE_CREATESIMPLEXNOISE_LAUNCH;
		diagrams[E_RENDER_VOL][E_STATE_CREATESIMPLEXNOISE_END] = E_STATE_COPYTOTEXTURE_LAUNCH;
	    diagrams[E_RENDER_VOL][E_STATE_COPYTOTEXTURE_END] = E_STATE_GENERATEVOLUME_LAUNCH;
	    diagrams[E_RENDER_VOL][E_STATE_GENERATEVOLUME_END] = E_STATE_LENGTH;

		curDiagram = diagrams[renderMethod];

		visPageSizeInUnits = singleton->visPageSizeInUnits;

		iVolumeSize = worldSizeInPages.getIX()*worldSizeInPages.getIY()*worldSizeInPages.getIZ();
		
	    worldData = new GamePage*[iVolumeSize];
	    
	      

		
		for (i = 0; i < iVolumeSize; i++) {
			worldData[i] = NULL;
		}

	    
	    #ifdef DEBUG_MODE
	    popTrace();
	    #endif
	}
void GameWorld::update (bool changesMade, bool bufferInvalid)
                                                          {


		bool procResult = processPages();
		bool doRenderGeom = false;


		if ( (lastProcResult != procResult) && (procResult == false)  ) {
			singleton->wsBufferInvalid = true;
		}

		if (procResult || changesMade) {
			renderPages();

			
			
			if ( (singleton->grassState != E_GRASS_STATE_ANIM) ) {
				renderGrass();
				doRenderGeom = true;

			}

			
		}

		if (singleton->grassState == E_GRASS_STATE_ANIM) {
			renderGrass();
			doRenderGeom = true;
			bufferInvalid = true;
		}


		if (procResult || changesMade || bufferInvalid || singleton->rbDown || singleton->lbDown) {


			if (singleton->mouseState == E_MOUSE_STATE_BRUSH) {
				doRenderGeom = true;
			}

			if (doRenderGeom) {
				renderGeom();
				combineBuffers();
			}
			

			glClearColor(0.6,0.6,0.7,0.0);
			glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

			postProcess();
			
			glutSwapBuffers();
			glFlush();
			
		}


		lastProcResult = procResult;
		
		
	}
bool GameWorld::processPages ()
                            {


		
		int counter;
		int i, j, k;
		int res;
		int ind;

		int iw = worldSizeInPages.getIX();
	    int jw = worldSizeInPages.getIY();
	    int kw = worldSizeInPages.getIZ();

	    int ii;
	    int jj;
	    int kk;

	    bool cmade = false;




	    camPagePos.copyFrom( &(singleton->cameraPos) );
		camPagePos.intDivXYZ(visPageSizeInPixels);

	    int curInd = 0;
		

	    int m;
	    E_STATES nState;

	    int loadRad = singleton->maxW;

	    int changeCount = 0;
	    int maxChanges = 32;

	    if (singleton->lbDown || singleton->rbDown) {
	    	loadRad = 1;
	    	//return false;
	    }
	    


	    // check for threads to free
	    if (availThreads < maxThreads) {
	    	for (i = 0; i < ocThreads.size(); i++) {
	    		if ( ocThreads[i] == -1) {
	    			// already freed
	    		}
	    		else{
	    			if ( worldData[ocThreads[i]] == NULL ) {
	    				// page was destroyed, free thread

	    				ocThreads[i] = -1;
	    				availThreads++;
	    			}
	    			else {
	    				if (worldData[ocThreads[i]]->threadRunning) {

	    				}
	    				else {
	    					ocThreads[i] = -1;
	    					availThreads++;
	    				}
	    			}
	    		}
	    	}
	    }

	    if (availThreads == 0) {
	    	return false;
	    }


		for (k = 0; k < singleton->maxH; k++) {
	    //for (k = -loadRad; k <= loadRad; k++) {
	    	kk = k;//+camPagePos.getIZ();

			for (j = -(loadRad); j <= (loadRad); j++) {
				jj = j+camPagePos.getIY();

				for (i = -(loadRad); i <= (loadRad); i++) {
					ii = i+camPagePos.getIX();

					
					

					if ( checkBounds(ii,jj,kk) ) {



						ind = kk*jw*iw + jj*iw + ii;

						curPos.setIXYZ(ii*visPageSizeInUnits,jj*visPageSizeInUnits,kk*visPageSizeInUnits);

						


						if (worldData[ind] == NULL) {

							#ifdef DEBUG_MODE
							doTrace("E_STATE_INIT_LAUNCH");
							#endif

							worldData[ind] = new GamePage();
							worldData[ind]->init(singleton, &curPos);

							pageCount++;
							changeCount++;


						}
						else {

							
							/*

							if (worldData[ind]->curState == E_STATE_CREATESIMPLEXNOISE_END_NOT_PARTIAL) {
								availThreads++;
								if (availThreads > maxThreads) {
									availThreads = maxThreads;
								}
								worldData[ind]->curState = E_STATE_LENGTH;
							}

							if (worldData[ind]->curState == E_STATE_CREATESIMPLEXNOISE_END_PARTIAL) {
								availThreads++;
								if (availThreads > maxThreads) {
									availThreads = maxThreads;
								}
								worldData[ind]->curState = E_STATE_CREATESIMPLEXNOISE_END;
								
							}

							*/

							nState = (E_STATES)curDiagram[worldData[ind]->curState];


							switch(nState) {
	                            case E_STATE_CREATESIMPLEXNOISE_LAUNCH:

	                            	#ifdef DEBUG_MODE
	                            	doTrace("E_STATE_CREATESIMPLEXNOISE_LAUNCH");
	                            	#endif

	                            	

	                            	if ( availThreads > 0 ) {

	                            		

	                            		counter = 0;
	                            		while (ocThreads[counter] != -1) {
	                            			counter++;
	                            			if (counter >= maxThreads) {
	                            				break;
	                            			}
	                            		}
	                            		if (counter >= maxThreads) {
	                            			
	                            		}
	                            		else {


	                            			ocThreads[counter] = ind;
	                            			
	                            			availThreads--;

	                            			worldData[ind]->nextState = nState;

	                            			try {
	                            				threadpool.start(*worldData[ind]);
	                            			}
	                            			catch (SystemException & exc) {
	                            				doTrace("MEM EXCEPTION");
	                            			}

	                            			changeCount++;

	                            		}


	                            		
	                            	}
	                            	else {
	                            		return cmade;
	                            	}
	                            	

	                            	//worldData[ind]->run();
									
									
								break;
								case E_STATE_COPYTOTEXTURE_LAUNCH:

									

									#ifdef DEBUG_MODE
									doTrace("E_STATE_COPYTOTEXTURE_LAUNCH");
									#endif

									
									worldData[ind]->nextState = nState;
									worldData[ind]->copyToTexture();
									cmade = true;
									
									changeCount++;

									//return cmade;
								break;


								case E_STATE_GENERATEVOLUME_LAUNCH:

									#ifdef DEBUG_MODE
									doTrace("E_STATE_GENERATE_VOLUME_LAUNCH");
									#endif

									
									worldData[ind]->nextState = nState;
									worldData[ind]->generateVolume();
									cmade = true;

									changeCount++;
									
									//return cmade;
								break;

								
								case E_STATE_WAIT:

									//do nothing
								break;
								case E_STATE_LENGTH:
	                            	
								break;
								default:
	                                
								break;

							}

							if (changeCount >= maxChanges) {
								return cmade;
							}
							
						}

					}

					curInd++;
				}
			}
		}



		
		return cmade;
	}
void GameWorld::renderPages ()
                           {

		int i, j, k;
		int res;
		int ind;

		int iw = worldSizeInPages.getIX();
	    int jw = worldSizeInPages.getIY();
	    int kw = worldSizeInPages.getIZ();

	    int ii;
	    int jj;
	    int kk;

	    int loadRad = 12;//singleton->maxW;

	    camPagePos.copyFrom( &(singleton->cameraPos) );
	    camPagePos.intDivXYZ(visPageSizeInPixels);

	    int curInd = 0;
		

	    int m;


	    singleton->bindShader("BlitShader");
	    singleton->bindFBO("pagesFBO");

	    

		for (k = 0; k < singleton->maxH; k++) {
			kk = k;//+camPagePos.getIZ();
		//for (k = -loadRad; k <= loadRad; k++) {
			//kk = k+camPagePos.getIZ();


			for (j = -loadRad; j <= loadRad; j++) {
				jj = j+camPagePos.getIY();

				for (i = -loadRad; i <= loadRad; i++) {
					ii = i+camPagePos.getIX();

					
					
					
					

					

					if ( checkBounds(ii,jj,kk) ) {



						ind = kk*jw*iw + jj*iw + ii;

						if (worldData[ind] == NULL) {

						}
						else {

							if (worldData[ind]->fillState == E_FILL_STATE_PARTIAL) {
								switch(curDiagram[worldData[ind]->curState]) {
									case E_STATE_LENGTH:
										drawPage(worldData[ind], ii, jj, kk);
									break;
								}
							}

							
							
						}

					}

					curInd++;
				}
			}
			

			
		}

		singleton->unbindShader();
		singleton->unbindFBO();
	    

		//doTrace( "POSSIBLE ERROR: " , i__s(glGetError()) , "\n" );

	}
void GameWorld::drawPage (GamePage * gp, int dx, int dy, int dz)
                                                            {

		int pitchSrc = (singleton->visPageSizeInPixels*2);
		int pitchSrc2 = (singleton->visPageSizeInPixels*2)/2;

		int dxmod = dx*pitchSrc2 - singleton->cameraPos.getIX();
		int dymod = dy*pitchSrc2 - singleton->cameraPos.getIY();
		int dzmod = dz*pitchSrc2 - singleton->cameraPos.getIZ();


		int x1 = (dxmod-dymod) - pitchSrc2;
		int y1 = (-(dxmod/2) + -(dymod/2) + dzmod) - pitchSrc2;
		int x2 = x1 + pitchSrc;
		int y2 = y1 + pitchSrc;

		float fx1 = x1;
		float fy1 = y1;
		float fx2 = x2;
		float fy2 = y2;

		
		// TODO: should be baseW/H?

		float sx = singleton->bufferDimHalf.getFX();
		float sy = singleton->bufferDimHalf.getFY();

		float myZoom = std::min(1.0f,singleton->cameraZoom);


		fx1 = fx1*(myZoom)/sx;
		fy1 = fy1*(myZoom)/sy;
		fx2 = fx2*(myZoom)/sx;
		fy2 = fy2*(myZoom)/sy;



	    singleton->sampleFBODirect(gp->fboSet);


	    glColor4f(1, 1, 1, 1);
	    glBegin(GL_QUADS);
	    glNormal3f(0, 0, 1);
	    
	    
	    glTexCoord2f(0.0f, 0.0f);
	    glVertex3f(fx1,fy1,0.0f);
	    
	    glTexCoord2f(1.0f, 0.0f);
	    glVertex3f(fx2,fy1,0.0f);
	    
	    glTexCoord2f(1.0f, 1.0f);
	    glVertex3f(fx2,fy2,0.0f);
	    
	    glTexCoord2f(0.0f, 1.0f);
	    glVertex3f(fx1,fy2,0.0f);
	    
	    glEnd();

	    singleton->unsampleFBODirect(gp->fboSet);



	}
void GameWorld::combineBuffers ()
                              {

		singleton->bindShader("CombineShader");

		singleton->bindFBO("combineFBO");
		singleton->sampleFBO("pagesFBO",0);
		singleton->sampleFBO("grassFBO",2);
		//singleton->sampleFBO("geomFBO",4);


		singleton->setShaderFloat("cameraZoom", singleton->cameraZoom);
		
		singleton->drawFSQuad(1.0f);

		//singleton->unsampleFBO("geomFBO",4);
		singleton->unsampleFBO("grassFBO",2);
		singleton->unsampleFBO("pagesFBO",0);
		
		singleton->unbindFBO();
		singleton->unbindShader();
	}
void GameWorld::renderGeom ()
                          {


		
		//glEnable(GL_DEPTH_TEST);

		singleton->bindShader("GeomShader");
		
		singleton->setShaderFloat("curTime", singleton->curTime);
		singleton->setShaderFloat("cameraZoom", singleton->cameraZoom);
		singleton->setShaderfVec3("cameraPos", &(singleton->cameraPos));
		singleton->setShaderfVec2("bufferDim", &(singleton->bufferDimHalf));
		
		singleton->bindFBO("geomFBO");
		//singleton->sampleFBO("pagesFBO");


		glEnable(GL_DEPTH_TEST);
		//remember 2x radius


		/*

		if (singleton->activeObject == E_OBJ_LIGHT) { //  || activeObject == E_OBJ_NONE singleton->activeObject == E_OBJ_CAMERA || 

		}
		else {

			
			if (singleton->mouseState == E_MOUSE_STATE_BRUSH) {
				singleton->setShaderFloat("matVal", 5.0f);
				singleton->drawCubeCentered(lastUnitPos, (singleton->curBrushRad)*(singleton->unitSizeInPixels)  );
			}
			

		}
		*/

		if (singleton->mouseState == E_MOUSE_STATE_BRUSH) {
			singleton->setShaderFloat("matVal", 5.0f);
			singleton->drawCubeCentered(lastUnitPos, (singleton->curBrushRad)*(singleton->unitSizeInPixels)  );
		}

		glClear(GL_DEPTH_BUFFER_BIT);




		if (singleton->mouseState == E_MOUSE_STATE_BRUSH) {
			singleton->setShaderFloat("matVal", 6.0f);
			singleton->drawCubeCentered(lastUnitPos, (singleton->curBrushRad)*(singleton->unitSizeInPixels)  );
		}
		else {
			if (singleton->showUI) {
				singleton->setShaderFloat("matVal", 4.0f);
				singleton->drawCubeCentered(singleton->lightPos,32.0);

				singleton->setShaderFloat("matVal", 5.0f);
				singleton->drawCubeCentered(singleton->fogPos,32.0);
			}	
		}


			

		

		glDisable(GL_DEPTH_TEST);

		

		//singleton->unsampleFBO("pagesFBO");
		singleton->unbindFBO();
		singleton->unbindShader();

		//glDisable(GL_DEPTH_TEST);

		
	}
void GameWorld::modifyUnit (FIVector4 * fPixelWorldCoordsBase, E_BRUSH brushAction)
                                                                               {

		int radius = singleton->curBrushRad;

		FIVector4 fPixelWorldCoords;
		fPixelWorldCoords.copyFrom(fPixelWorldCoordsBase);

		float selMod;


		selMod = 0.0f;

		/*
		if (brushAction == E_BRUSH_ADD) {
			fPixelWorldCoords.addXYZ(0.0,0.0,radius*singleton->unitSizeInPixels);
		}
		if (brushAction == E_BRUSH_SUB) {
			fPixelWorldCoords.addXYZ(0.0,0.0,-radius*singleton->unitSizeInPixels);
		}
		*/

		

		


		int iw = worldSizeInPages.getIX();
		int jw = worldSizeInPages.getIY();
		int kw = worldSizeInPages.getIZ();
		int ind;
		int ind2;
		int pageSizeMult = visPageSizeInUnits*(singleton->bufferMult);


		int i, j, k, m, n, o, p;

		int ii;
		int jj;
		int kk;

		int pixelPS = (singleton->unitSizeInPixels*singleton->visPageSizeInUnits);



		lastUnitPos.copyFrom(&fPixelWorldCoords);
		lastUnitPos.intDivXYZ(singleton->unitSizeInPixels);
		lastUnitPos.multXYZ(singleton->unitSizeInPixels);
		lastUnitPos.setFW(singleton->unitSizeInPixels);

		lastPagePos.copyFrom(&fPixelWorldCoords);
		lastPagePos.intDivXYZ(pixelPS);
		lastPagePos.multXYZ(pixelPS);
		lastPagePos.setFW(pixelPS);

		bool changes = false;

		int newRad = 2 + radius/visPageSizeInUnits;


		pagePos.copyFrom(&fPixelWorldCoords);
		unitPos.copyFrom(&fPixelWorldCoords);

		pagePos.intDivXYZ( pixelPS );
		unitPos.intDivXYZ( singleton->unitSizeInPixels );


		unitPosMin.copyFrom(&unitPos);
		unitPosMax.copyFrom(&unitPos);

		unitPosMin.addXYZ((float)radius, -1.0f);
		unitPosMax.addXYZ((float)radius, 1.0f);


		if (brushAction == E_BRUSH_MOVE) {
			return;
		}



		
		for (m = 0; m < 2; m++) {
			for (i = -newRad; i <= newRad; i++) {
				for (j = -newRad; j <= newRad; j++) {
					for (k = -newRad; k <= newRad; k++) {




						ii = i+pagePos.getIX();
						jj = j+pagePos.getIY();
						kk = k+pagePos.getIZ();

						if (checkBounds(ii,jj,kk)) {
							ind = kk*jw*iw + jj*iw + ii;

							if (worldData[ind] == NULL) {

								curPos.setIXYZ(ii*visPageSizeInUnits,jj*visPageSizeInUnits,kk*visPageSizeInUnits);
								worldData[ind] = new GamePage();
								worldData[ind]->init(singleton, &curPos);
								worldData[ind]->createSimplexNoise();

								//doTrace("created new page");
								//worldData[ind]->curState = E_STATE_LENGTH;
							}

							if (
								//(worldData[ind]->curState != E_STATE_CREATESIMPLEXNOISE_BEG) // ||
								//(worldData[ind]->curState == E_STATE_LENGTH)
								true
							) {
								

								startBounds.maxXYZ(&unitPosMin,&(worldData[ind]->worldUnitMin));
								endBounds.minXYZ(&unitPosMax,&(worldData[ind]->worldUnitMax));

								

								

								



								if (
									(startBounds.getFX() > endBounds.getFX()) || 
									(startBounds.getFY() > endBounds.getFY()) ||
									(startBounds.getFZ() > endBounds.getFZ())
								) {
									
								}
								else {

									


									for (n = startBounds.getIX(); n < endBounds.getIX(); n++) {
										for (o = startBounds.getIY(); o < endBounds.getIY(); o++) {
											for (p = startBounds.getIZ(); p < endBounds.getIZ(); p++) {

												tempVec.setIXYZ(n,o,p);

												if (
													tempVec.inBoundsXYZ(
														&(worldData[ind]->worldUnitMin),
														&(worldData[ind]->worldUnitMax)
													)
												) {
													tempVec.addXYZRef( &(worldData[ind]->worldUnitMin), -1.0);

													ind2 = tempVec.getIZ()*pageSizeMult*pageSizeMult + tempVec.getIY()*pageSizeMult + tempVec.getIX();
													
													if (ind2 < 0 || ind2 >= pageSizeMult*pageSizeMult*pageSizeMult) {
														//doTrace("ind2 out of range ", i__s(ind2), " of ", i__s(pageSizeMult*pageSizeMult*pageSizeMult));
													}
													else {

														if (m == 0) {


															if (brushAction == E_BRUSH_SUB || p >= singleton->maxHeightInUnits) {
																worldData[ind]->volData[ind2] &= (255<<16)|(255<<8)|(255);
															}
															else {
																worldData[ind]->volData[ind2] |= (255<<24);
															}

															worldData[ind]->isDirty = true;
															changes = true;
														}
														else {
															if (worldData[ind]->isDirty) {
																worldData[ind]->isDirty = false;
																worldData[ind]->copyToTexture();
																worldData[ind]->generateVolume();
															}
														}

													}

												}


											}
										}
									}
								}

								


								

							}
							
						}


						




						

						

					}
				}
			}
		}

		if (changes) {
			singleton->changesMade=true;
		}

		
		

	}
void GameWorld::renderWorldSpace ()
                                {
		doTrace("renderWorldSpace() TOT GPU MEM USED (MB): ", f__s(TOT_GPU_MEM_USAGE));

		singleton->wsBufferInvalid = false;

		
		singleton->worldToScreen(&screenCoords, &(singleton->lightPos));

		singleton->bindShader("WorldSpaceShader");
		

		singleton->bindFBO("worldSpaceFBO");
		singleton->sampleFBO("pagesFBO",0);
		singleton->sampleFBO("geomFBO",2);

		
		//MUST BE CALLED AFTER FBO IS BOUND
		singleton->setShaderVec2("resolution",singleton->currentFBOResolutionX, singleton->currentFBOResolutionY);
		singleton->setShaderVec2("mouseCoords",singleton->mouseX,singleton->mouseY);
		singleton->setShaderfVec3("cameraPos", &(singleton->cameraPos));
		singleton->setShaderfVec4("fogPos", &(singleton->fogPos));
		singleton->setShaderfVec3("lightPosWS", &(singleton->lightPos));
		singleton->setShaderfVec2("lightPosSS", &screenCoords);
		singleton->setShaderFloat("cameraZoom",singleton->cameraZoom);
		singleton->setShaderfVec2("bufferDim", &(singleton->bufferDimHalf) );


		singleton->drawFSQuad(1.0f);

		
		singleton->unsampleFBO("geomFBO",2);
		singleton->unsampleFBO("pagesFBO",0);
		

		singleton->unbindFBO();
		singleton->unbindShader();

		float newZoom = std::max(1.0f,singleton->cameraZoom);
		singleton->drawFBO("resultFBO", 0, newZoom );

		FBOWrapper* fbow = singleton->getFBOWrapper("worldSpaceFBO",0);

		fbow->getPixels();

	}
void GameWorld::renderGrass ()
                           {


		float curTime = 0.0;

		if (singleton->grassState == E_GRASS_STATE_ANIM) {
			curTime = singleton->curTime;
		}

		
		//glEnable(GL_DEPTH_TEST);

		singleton->bindShader("GrassShader");
		
		singleton->setShaderFloat("curTime", curTime);
		singleton->setShaderFloat("grassHeight", singleton->grassHeight);
		singleton->setShaderFloat("cameraZoom", singleton->cameraZoom);
		singleton->setShaderfVec3("cameraPos", &(singleton->cameraPos));
		singleton->setShaderfVec2("bufferDim", &(singleton->bufferDimHalf));

		
		singleton->bindFBO("grassFBO");
		singleton->sampleFBO("pagesFBO");

		if (singleton->grassState == E_GRASS_STATE_ANIM || singleton->grassState == E_GRASS_STATE_ON) {
			glCallList(singleton->grassTris);
		}
		

		singleton->unsampleFBO("pagesFBO");
		singleton->unbindFBO();
		singleton->unbindShader();

		//glDisable(GL_DEPTH_TEST);

		
	}
void GameWorld::postProcess ()
                           {

		// NOTE: ALWAYS UNSAMPLE IN REVERSE ORDER!!!


		//singleton->drawFBO("worldSpaceFBO", 0, 1.0 );

		

		singleton->worldToScreen(&screenCoords, &(singleton->lightPos));

		singleton->bindShader("LightingShader");
		singleton->setShaderVec2("mouseCoords",singleton->mouseX,singleton->mouseY);
		singleton->setShaderfVec3("cameraPos", &(singleton->cameraPos));
		singleton->setShaderfVec3("lightPosWS", &(singleton->lightPos));
		singleton->setShaderfVec4("fogPos", &(singleton->fogPos));
		singleton->setShaderfVec4("activeObjectPos", &(singleton->activeObjectPos));
		singleton->setShaderfVec2("lightPosSS", &screenCoords);
		singleton->setShaderfVec4("lastUnitPos", &(lastUnitPos) );
		singleton->setShaderfVec4("lastPagePos", &(lastPagePos) );




		singleton->setShaderFloat("cameraZoom",singleton->cameraZoom);
		singleton->setShaderfVec2("bufferDim", &(singleton->bufferDimHalf));
		singleton->setShaderFloat("diskOn",singleton->diskOn);
		singleton->setShaderFloat("curTime", singleton->curTime);

		
		singleton->bindFBO("resultFBO");
		singleton->sampleFBO("combineFBO",0);
		singleton->sampleFBO("geomFBO", 2);

		//MUST BE CALLED AFTER FBO IS BOUND
		singleton->setShaderVec2("resolution",singleton->currentFBOResolutionX, singleton->currentFBOResolutionY);

		singleton->drawFSQuad(1.0f);
		singleton->unsampleFBO("geomFBO", 2);
		singleton->unsampleFBO("combineFBO",0);
		singleton->unbindFBO();
		singleton->unbindShader();

		float newZoom = std::max(1.0f,singleton->cameraZoom);
		singleton->drawFBO("resultFBO", 0, newZoom );

		
		
	}
GameWorld::~ GameWorld ()
                     {


		threadpool.stopAll();

		int i;

		for (i = 0; i < iVolumeSize; i++) {
			if (worldData[i]) {
				delete worldData[i];
			}
			
		}
		
	}
#undef LZZ_INLINE
 
