void buck_Enable(){                                                                  //Enable MPPT Buck Converter
  buckEnable = 1;
  digitalWrite(buck_EN,HIGH);
  digitalWrite(LED,HIGH);
}
void buck_Disable(){                                                                 //Disable MPPT Buck Converter
  buckEnable = 0; 
  digitalWrite(buck_EN,LOW);
  digitalWrite(LED,LOW);
  PWM = 0;
}

void predictivePWM(){                                                                //PREDICTIVE PWM ALGORITHM 
  if(voltageInput<=0){PPWM=0;}                                                       //Prevents Indefinite Answer when voltageInput is zero
  // else{PPWM =(PPWM_margin*pwmMax*voltageOutput)/(100.00*voltageInput)-100;}              //Compute for predictive PWM Floor and store in variable
  else{PPWM =(voltageOutput/voltageInput)*100;}              //Compute for predictive PWM Floor and store in variable
  PPWM = constrain(PPWM,0,PWM_MaxDC);
}   

void PWM_Modulation(){
  if(output_Mode==0){PWM = constrain(PWM,0,PWM_MaxDC);}                          //PSU MODE PWM = PWM OVERFLOW PROTECTION (limit floor to 0% and ceiling to maximim allowable duty cycle)
  else{
    predictivePWM();                                                                 //Runs and computes for predictive pwm floor
    //PWM = constrain(PWM,PPWM,pwmMaxLimited);                //CHARGER MODE PWM - limit floor to PPWM and ceiling to maximim allowable duty cycle)
    PWM = constrain(PWM,PPWM,PWM_MaxDC);                //CHARGER MODE PWM - limit floor to PPWM and ceiling to maximim allowable duty cycle)
  } 
  // ledcWrite(pwmChannel,PWM);                                                      //Set PWM duty cycle and write to GPIO when buck is enabled
  // duty_PWM = maping(PWM, 0, 2047, 0, 100);
  mcpwm_set_duty(MCPWM_UNIT_0, MCPWM_TIMER_0, MCPWM_OPR_A, PWM);
  mcpwm_set_duty(MCPWM_UNIT_0, MCPWM_TIMER_0, MCPWM_OPR_B, PWM);
  buck_Enable();                                                                     //Turn on MPPT buck (IR2104)
}
     
void Charging_Algorithm(){
  if(ERR>0||chargingPause==1){                                                       //ERROR PRESENT  - Turn off MPPT buck when error is present or when chargingPause is used for a pause override
    buck_Disable();
    if(OTE){
      lcd.setCursor(0,0);lcd.print("================");                              //Display LCD message 
      lcd.setCursor(0,1);lcd.print("     ERROR      ");                              //Display LCD message
      lcd.setCursor(0,2);lcd.print("OVER TEMPERATURE");                              //Display LCD message
      lcd.setCursor(0,3);lcd.print("================");                              //Display LCD message
      delay(1000);
      lcd.clear(); 
    }
    else if(IOC){
      lcd.setCursor(0,0);lcd.print("================");                              //Display LCD message 
      lcd.setCursor(0,1);lcd.print("     ERROR      ");                              //Display LCD message
      lcd.setCursor(0,2);lcd.print("IN  OVER CURRENT");                              //Display LCD message
      lcd.setCursor(0,3);lcd.print("================");                              //Display LCD message
      delay(1000);
      lcd.clear(); 
    }
    else if(OOC){
      lcd.setCursor(0,0);lcd.print("================");                              //Display LCD message 
      lcd.setCursor(0,1);lcd.print("     ERROR      ");                              //Display LCD message
      lcd.setCursor(0,2);lcd.print("OUT OVER CURRENT");
      lcd.setCursor(0,3);lcd.print("================");                              //Display LCD message
      delay(1000);
      lcd.clear();
    }
    else if(OOV && !REC){
      lcd.setCursor(0,0);lcd.print("================");                              //Display LCD message 
      lcd.setCursor(0,1);lcd.print("     ERROR      ");                              //Display LCD message
      lcd.setCursor(0,2);lcd.print("OUT OVER VOLTAGE");                              //Display LCD message
      lcd.setCursor(0,3);lcd.print("================");                              //Display LCD message
      delay(1000);
      lcd.clear();
    }
  }
  else{
    if(REC==1){                                                                      //IUV RECOVERY - (Only active for charging mode)
      REC=0;                                                                         //Reset IUV recovery boolean identifier 
      // digitalWrite(relay_BAT, LOW);
      buck_Disable();                                                                //Disable buck before PPWM initialization
      lcd.setCursor(0,0);lcd.print("  POWER SOURCE  ");                              //Display LCD message
      lcd.setCursor(0,1);lcd.print("    DETECTED    ");                              //Display LCD message
      lcd.setCursor(0,2);lcd.print("================");                              //Display LCD message 
      Serial.println("> Solar Panel Detected");                                      //Display serial message
      Serial.print("> Computing For Predictive PWM ");                               //Display serial message 
      for(int i = 0; i<40; i++){Serial.print(".");delay(30);}                        //For loop "loading... effect
      Serial.println("");                                                            //Display a line break on serial for next lines
      Read_Sensors();
      // voltagePvSystem = voltageInput;
      predictivePWM();
      PWM = PPWM;
      lcd.clear();
      // digitalWrite(relay_BAT, HIGH);
    }
    
    else{                                                                           //NO ERROR PRESENT  - Continue power conversion
      /////////////////////// CC-CV BUCK PSU ALGORITHM ////////////////////////////// 
      if(MPPT_Mode==0){                                                             //CC-CV PSU MODE
        if(currentOutput>currentCharging)       {PWM-= 0.1;}                        //Current Is Above → Decrease Duty Cycle
        else if(voltageOutput>voltageBatteryMax){PWM-= 0.1;}                        //Voltage Is Above → Decrease Duty Cycle   
        else if(voltageOutput<voltageBatteryMax){PWM+= 0.1;}                        //Increase duty cycle when output is below charging voltage (for CC-CV only mode)
        else {}                                                                     //Do nothing when set output voltage is reached 
        PWM_Modulation();                                                           //Set PWM signal to Buck PWM GPIO
      }
      /////////////////////// MPPT & CC-CV CHARGING ALGORITHM ///////////////////////  
      else{                                                                                                                                                         
        if(currentOutput>currentCharging){PWM -= 0.1;}                                      //Current Is Above → Decrease Duty Cycle
        else if(voltageOutput>voltageBatteryMax){PWM -= 0.1;}                               //Voltage Is Above → Decrease Duty Cycle   
        // else if(/*voltageInput>30 &&*/ currentOutput>0.3){PWM -= 1.0;}
        else{                                                                               //MPPT ALGORITHM
          if(powerInput>powerInputPrev && voltageInput>voltageInputPrev){PWM -= 0.1;}       // {PWM--;}  //  ↑P ↑V ; →MPP  //D--
          else if(powerInput>powerInputPrev && voltageInput<voltageInputPrev){PWM += 0.1;}  // {PWM++;}  //  ↑P ↓V ; MPP←  //D++
          else if(powerInput<powerInputPrev && voltageInput>voltageInputPrev){PWM += 0.1;}  // {PWM++;}  //  ↓P ↑V ; MPP→  //D++
          else if(powerInput<powerInputPrev && voltageInput<voltageInputPrev){PWM -= 0.1;}  // {PWM--;}  //  ↓P ↓V ; ←MPP  //D--
          else if(voltageOutput<voltageBatteryMax)                           {PWM += 0.1;}  // {PWM++;}  //  MP MV ; MPP Reached - 
          powerInputPrev   = powerInput;                                                    //Store Previous Recorded Power
          voltageInputPrev = voltageInput;                                                  //Store Previous Recorded Voltage  
        }
        // voltageInputPrev1= voltageInput;
        PWM_Modulation();                                                                   //Set PWM signal to Buck PWM GPIO
      }  
    }
  }
}
