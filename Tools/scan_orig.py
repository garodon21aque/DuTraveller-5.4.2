import re,glob,os,sys
for f in sorted(glob.glob('C:/Temp/orig_assets/*.uasset')):
    d=open(f,'rb').read()
    hits={}
    for n in ['AttachedActor','Health','MaxHealth','Mana','Level','Item','InventoryWidget','EnemyActor','DataTable','QuestID','CurrentObjectiveProgress','CurrentQuests','BP_QuestLog','ProgressVisible','BP_Pickup']:
        c=len(re.findall(n.encode(),d))
        if c: hits[n]=c
    print(os.path.basename(f),hits)
