<?xml version="1.0" encoding="UTF-8"?>
<tileset version="1.10" tiledversion="1.12.2" name="props" tilewidth="76" tileheight="321" tilecount="13" columns="0">
 <grid orientation="orthogonal" width="1" height="1"/>
 <tile id="1">
  <properties>
   <property name="collision" type="bool" value="true"/>
  </properties>
  <image source="../images/props/platform2.png" width="48" height="21"/>
 </tile>
 <tile id="6">
  <properties>
   <property name="collision" type="bool" value="true"/>
  </properties>
  <image source="../images/props/pillar1.png" width="64" height="112"/>
 </tile>
 <tile id="7">
  <properties>
   <property name="collision" type="bool" value="true"/>
  </properties>
  <image source="../images/props/platform3.png" width="44" height="11"/>
 </tile>
 <tile id="8">
  <properties>
   <property name="collision" type="bool" value="true"/>
  </properties>
  <image source="../images/props/platform1.png" width="46" height="13"/>
 </tile>
 <tile id="3">
  <image source="../images/props/pillar2.png" width="76" height="164"/>
 </tile>
 <tile id="5">
  <image source="../images/props/glow.png" width="60" height="61"/>
 </tile>
 <tile id="9">
  <image source="../images/props/pillar3.png" width="35" height="321"/>
 </tile>
 <tile id="10">
  <image source="../images/props/hang_light.png" width="60" height="228"/>
 </tile>
 <tile id="19">
  <properties>
   <property name="damage" type="int" value="1"/>
   <property name="hp" type="int" value="1"/>
  </properties>
  <image source="../images/characters/enemy/bat_idle1.png" width="32" height="32"/>
  <animation>
   <frame tileid="19" duration="250"/>
   <frame tileid="20" duration="250"/>
   <frame tileid="21" duration="250"/>
   <frame tileid="22" duration="250"/>
  </animation>
 </tile>
 <tile id="20">
  <image source="../images/characters/enemy/bat_idle2.png" width="32" height="32"/>
 </tile>
 <tile id="21">
  <image source="../images/characters/enemy/bat_idle3.png" width="32" height="32"/>
 </tile>
 <tile id="22">
  <image source="../images/characters/enemy/bat_idle4.png" width="32" height="32"/>
 </tile>
 <tile id="26">
  <properties>
   <property name="damage" type="int" value="1"/>
  </properties>
  <image source="../images/props/spike.png" width="16" height="16"/>
 </tile>
</tileset>
