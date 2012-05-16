#include <stdio.h>
#include <stdlib.h>
#include <cc3.h>
#include <cc3_ilp.h>
//#include "cc3_tls.h"
#include <../hal/lpc2106-cmucam3/serial.h>
//#include <cc3_histogram.c>




#define PAS_X 15
#define PAS_Y 15
#define NB_COL 24

#define TAILLE_MIN 3
#define MAX_SEGMENT 120
#define MAX_FIGURE 200
#define SEUIL_HR 35
#define SEUIL_BR 15
#define SEUIL_SUB2 30
#define SEUIL_SUB4 60

#define GAUCHE 0
#define DROIT 1

#define LARGEUR_MIN 3

inline void cc3_rgb2tls (cc3_pixel_t * pix);
uint8_t estJaune(cc3_pixel_t pix);
uint8_t estRouge(cc3_pixel_t pix);
uint8_t estBleu(cc3_pixel_t pix);
uint8_t estVert(cc3_pixel_t pix);
uint8_t estNoir(cc3_pixel_t pix);
uint8_t estBlanc(cc3_pixel_t pix);
uint8_t estCouleur(cc3_pixel_t pix,uint8_t couleur);


typedef struct  {
  uint16_t x0,x1,y0,y1;
  uint8_t couleurs[2]; // 0 indéfini, 1 rouge, 2 bleu
  uint8_t nb_bleu[2];
  uint8_t nb_rouge[2];
  uint8_t id; // 0 libre, 1 en cours, 2 terminée et valide
} figure_t;

typedef struct  {
  uint16_t x0,x1;
//  uint8_t couleurs[2];
  figure_t *pFigure;
} segment_t;



int main (void)
{

  // Ligne de pixel de l'image
  cc3_pixel_t *ligne;
  // Tableau des segments
  segment_t segments_anciens[MAX_SEGMENT];
  segment_t segments_courant[MAX_SEGMENT];
  // Tableau des figures
  figure_t figures[MAX_FIGURE];
  
  // Index
  uint8_t index_segment_a=0;
  uint8_t index_segment_c=0;
  uint16_t index_figure =0; 
  
  uint8_t couleur='P'; // P => Pion, R => rouge, B => bleu
  uint8_t mode='I'; // I => Init, G => recherche Globale, T => tracking
  uint8_t traitement_image = 1; // 0 => on n'analyse pas l'images, 1 on analyse l'image
  uint8_t en_attente = 0; // dire qu'on attend une réponse de la part du pic
  uint16_t track_x=0;   // Point du centre de l'ancienne figure 
  uint16_t track_y=0;   // Point du centre de l'ancienne figure 
  uint8_t haute_resolution=1; // haute résolution
  
  // Reception série
  int recu=-1;
  
  // Pour l'optimisation
  uint8_t pas_x=1;
  uint8_t pas_y=1;

  // ouvreture de l'UART
  cc3_uart_init (0,
                 CC3_UART_RATE_115200,
                 CC3_UART_MODE_8N1, CC3_UART_BINMODE_TEXT);
                 
                 
  // Make it so that stdout and stdin are not buffered
//  setbuf(stdout, NULL);
//  setbuf(stdin, NULL);

  // Init de la caméra (probablement)
  cc3_camera_init ();


  //cc3_camera_set_colorspace(CC3_COLORSPACE_YCRCB);

  // Résolution de la Caméra
  cc3_camera_set_resolution (CC3_CAMERA_RESOLUTION_LOW);
  //cc3_camera_set_resolution (CC3_CAMERA_RESOLUTION_HIGH);
  cc3_pixbuf_frame_set_subsample(CC3_SUBSAMPLE_NEAREST, 4, 4);

  // init pixbuf with width and height
  // When using the virtual-cam, note that this will skip the first image
  cc3_pixbuf_load ();
  
  
  // On alloue la mémoire pour stocker la ligne à traiter
  ligne = (cc3_pixel_t *) cc3_malloc_rows(1);

// On est prêt, on fait coucou !
  cc3_led_set_state(2,true);
  cc3_timer_wait_ms(500);
  cc3_led_set_state(2,false);

  


  while (true) {
    uint16_t k;
    uint16_t id_ligne;
    // Configuration de la prise de vue
    // Uniquement si on est en mode tracking ###
    
    if(traitement_image == 1){
      
      // On prend la photo !
      cc3_pixbuf_load ();
      
      index_figure =0;
      for (k = 0; k < MAX_FIGURE; k++){
        figures[k].x0 = 999;
        figures[k].id = 0; // Les figures sont inutilisées
        figures[k].nb_bleu[0]=0;
        figures[k].nb_bleu[1]=0;
        figures[k].nb_rouge[0]=0;
        figures[k].nb_rouge[1]=0;
      }
	  index_segment_a = 0;
      id_ligne = 0;
      while(cc3_pixbuf_read_rows(ligne,1)){
        // On récupère une ligne et on la traite
        // tant qu'on n'a pas épuisé les lignes de l'image
        uint16_t i,j;
        // On initialise les segments 
        for (i = 0; i < MAX_SEGMENT; i++){
          segments_courant[i].x0 = 999;
          segments_courant[i].pFigure = NULL;
        }
        index_segment_c = 0;
        
        
        // On traite chaque pixel
        for (i = 0; i < cc3_g_pixbuf_frame.width; i++){
          cc3_rgb2tls (&ligne[i]); // Passage en teinte - luminance - saturation
          
          // Condition liée à la couleur recherchée
          if(estCouleur(ligne[i],couleur)){
            // On ajoute le pixel à un segment.
            // On obtient le dernier segment
            segments_courant[index_segment_c].x0 = i; // Bord gauche
            //On parcourt le segment jusqu'au bout :
            i++;
            while(i<cc3_g_pixbuf_frame.width){
              cc3_rgb2tls (&ligne[i]); // Passage en teinte - luminance - saturation
              // Condition liée à la couleur recherchée
              if( estCouleur(ligne[i],couleur)){
                  i++;
              }else{
                  break;
              }
            }
            segments_courant[index_segment_c].x1 = (i-1); // Bord droit
            
            
            index_segment_c++; // On se prépare pour le segment suivant.

                 
          }
        }
        
        // On a traité la ligne, 
        // Il faut obtenir les couleurs encadrante du segment !
        
        // On attribue les segments aux figures existantes 
        // Ou on crée de nouvelles figures
        // Pour chaque segment
        j = 0;
        while(j < index_segment_c){
          
          
          i = 0;
          while(i < index_segment_a){
            // Si le segment courant a une portion commune avec un segment ancien
            if(segments_courant[j].x0 < segments_anciens[i].x1 &&
               segments_courant[j].x1 > segments_anciens[i].x0 ){
               // On signal que ce segment appartient à la igure en question
               segments_courant[j].pFigure = segments_anciens[i].pFigure;
               // On actualise la figure
               // bas
               segments_courant[j].pFigure->y1 = id_ligne;
               // Gauche
               if (segments_courant[j].pFigure->x0 > segments_courant[j].x0){
                 segments_courant[j].pFigure->x0 =segments_courant[j].x0;
               }
               // Droite
               if (segments_courant[j].pFigure->x1 < segments_courant[j].x1){
                 segments_courant[j].pFigure->x1 =segments_courant[j].x1;
               }
               
              
               
               
               break; // Pas la peine de continuer à tester les autres segments.
            }
            
            i++;
          }

          if(segments_courant[j].pFigure == NULL){
            // On doit alors créer une nouvelle figure.
            // On trouve un emplacement libre dans le tableau.
            if(index_figure == 0){
              if(figures[0].id != 0){
                index_figure++;
              }
            }else{
              while( (figures[index_figure].id != 0) &&  (index_figure < MAX_FIGURE-1) ){
                index_figure++;
              }
            }
            segments_courant[j].pFigure = &figures[index_figure];
            
            // Premier remplissage de la figure
            figures[index_figure].x0 = segments_courant[j].x0;
            figures[index_figure].x1 = segments_courant[j].x1;
            figures[index_figure].y0 = id_ligne ;
            figures[index_figure].y1 = id_ligne ;
            //figures[index_figure].couleurs[0] = segments_courant[j].couleurs[0];
            //figures[index_figure].couleurs[1] = segments_courant[j].couleurs[1];
            figures[index_figure].id = 1; // Figure en cours de création
          }
          
          j++;
        }
        
        // Faire le ménage, virer les parasytes
        // On parcourt toutes les figures
        i=0;
        while(j<MAX_FIGURE){
        // On cherche les figures qui sont terminées (=> y1 !=id_ligne)
        // Et qui n'ont pas déjà été vérifiées
          if(figures[j].id == 1 && figures[j].y1 != id_ligne ){
          
              // Appliquer les critères finaux en plus de ceux de taille ###
              if(figures[j].y1 - figures[j].y0 < TAILLE_MIN &&
                 figures[j].x1 - figures[j].x0 < TAILLE_MIN ){
                  figures[j].id = 0; // La figure est libre
                  if(j < index_figure){
                      index_figure = j;
                  }
              }else{
                  figures[j].id = 2; // La figure est terminée et on la garde !
              }
          }
          j++;
        }
        
        // Copier segment_courant dans segment_ancien
        index_segment_a = 0;
        while(index_segment_a < index_segment_c){
          segments_anciens[index_segment_a] = segments_courant[index_segment_a];
          index_segment_a++;
        }      
        id_ligne++;
      } // Fin traitement ligne par ligne
      
      
      traitement_image = 0;
    } // Fin traitement_image
    
    
    uint16_t i;
    uint16_t largeur;
    
    

    // I => Init, G => recherche Globale, T => tracking
    switch(mode){
    case 'J':
        printf("r\n");
        mode='I';
    case 'I':
        // Mode Init, on attend une instruction de couleur
        recu = uart0_getc_nb();
        if(recu!=-1){
            switch(recu){
            case 'P' :
            case 'B' :
            case 'R' :
            case 'W' :
                // Choix de la couleur
                couleur = recu;
                // Passage en mode recherche globale
                mode = 'G';
                traitement_image = 1; // On analyse la prochaine image
                en_attente =0;
                break;
            default :
                if(recu != '\n' && recu != '\r')
                    printf("%c\n",recu);
                break;
                
            }
            while(uart0_getc_nb() != -1); // Dans tous les cas, on vide le tampon d'entrée.
        }
        break;
    case 'G':
        // Mode global
        recu = uart0_getc_nb();
        if(recu!=-1){
            switch(recu){
            case ' ':
            case 's':
                // passage à la figure suivante
                traitement_image = 0; // On n'analyse pas l'image, on garde nos figures
                en_attente = 0; // C'est à la CMUcam de répondre
                break;
            case '/':
                // nouvelle recherche
                traitement_image = 1; // On refait une recherche d'image
                en_attente = 0; // C'est à la CMUcam de répondre
                break;
            case '~':
                mode = 'J';
                traitement_image = 0;
                break;
            default:
                // Il s'agit probablement du choix de la figure.
                if(recu >= '0' && recu <= '9'){
                  index_figure = 0;
                  while(recu >= '0' && recu <= '9'){
                      index_figure *= 10;
                      index_figure += recu - '0';
                      printf("Id = %d\n", index_figure);
                      recu = uart0_getc();
                  }
                  printf("Id = %d", index_figure);
                  track_x = (figures[index_figure].x0 + figures[index_figure].x1)/2;
                  track_y = (figures[index_figure].y0 + figures[index_figure].y1)/2;
                  mode = 'T';
                  traitement_image=1;
                }else if(recu != '\n' && recu != '\r'){
                    printf("%c\n",recu);
                }
                break;
            }
            // On vide le tampon d'entrée
            while(uart0_getc_nb() != -1);
        }
        if(en_attente == 0){
        // On doit envoyer qqch
          if(mode == 'G'){
            // On vérifie qu'on n'a pas changé de mode sur instruction du PIC
            // Si on ne doit pas traiter l'image, c'est qu'il faut envoyer la figure suivante
            if(traitement_image == 0){
              index_figure = MAX_FIGURE;
              largeur = LARGEUR_MIN;
              for(i=0;i<MAX_FIGURE;i++){
                
                if(figures[i].id > 0){
                  if(figures[i].x1 - figures[i].x0 > largeur){
                       index_figure = i;
                       largeur = figures[i].x1 - figures[i].x0;
                       figures[i].id = 0; // On ne renverra pas cette figure.
                    }
                }
                
              }
              
              if(index_figure < MAX_FIGURE){
                printf("g %d %d %d %d %d\n",figures[index_figure].x0*8,figures[index_figure].y0*8,
                     figures[index_figure].x1*8,figures[index_figure].y1*8,index_figure);

              }else{
                printf("g 0 0 0 0 0\n");
              }
              en_attente = 1;
            }
          }
        }
        break;
    case 'T':
        // Mode tracking
        // Il faut traiter la prochaine image
        traitement_image = 1;
        recu = uart0_getc_nb();
        if(recu!=-1){
            switch(recu){
            case '~':
                mode = 'J';
                traitement_image = 0;
                break;
            case '/':
                // retour à la recherche globale
                mode = 'G';
                traitement_image = 1; // On analyse la prochaine image

                en_attente = 0; // C'est à la CMUcam de répondre
            }
            // On vide le tampon d'entrée
            while(uart0_getc_nb() != -1);
        }
        // Envoie de la figure ici
        // Plus large figure contenant le point (track_x, track_y)
        largeur = 0;
        index_figure = MAX_FIGURE;
        for(i=0;i<MAX_FIGURE;i++){
          if(figures[i].id > 0){
            if( (figures[i].x1 > track_x) && (figures[i].x0 < track_x) &&
                (figures[i].y1 > track_y) && (figures[i].y0 < track_y) ){ 
                if((figures[i].x1 - figures[i].x0) > largeur){ // Plus large que la figure précédente retenue
                  index_figure = i;
                  largeur = figures[i].x1 - figures[i].x0;
                }
            }
          }
        }
        if(index_figure < MAX_FIGURE){

          // mise à jour de track_x, track_y
          track_x = (figures[index_figure].x0 + figures[index_figure].x1)/2 * pas_x;
          track_y = (figures[index_figure].y0 + figures[index_figure].y1)/2 * pas_y;
		    figures[index_figure].x0 *= 8;
		    figures[index_figure].x1 *= 8;
		    figures[index_figure].y0 *= 8;
		    figures[index_figure].y1 *= 8;
		    
          
          printf("t %d %d %d %d\n",figures[index_figure].x0,figures[index_figure].y0,
                     figures[index_figure].x1,figures[index_figure].y1);
          
        }else{
          printf("t 0 0 0 0\n");
        }
        break;
    } // Fin switch (mode)
    
    
    /// Fin du bloc ###
    
  } // Fin while(1)


}
uint8_t estCouleur(cc3_pixel_t pix,uint8_t couleur){
    switch(couleur){
        case 'P':
            return estJaune(pix);
            break;
        case 'B':
            return estBleu(pix);
            break;
        case 'R':
            return estRouge(pix);
            break;
        case 'W':
            return estBlanc(pix);
            break;
        default:
            return 0;
    }
}

uint8_t estJaune(cc3_pixel_t pix){
  if(pix.channel[CC3_CHANNEL_SAT] > 45){
    if(pix.channel[CC3_CHANNEL_VAL] > 80){
      if(pix.channel[CC3_CHANNEL_HUE] <= 64 && pix.channel[CC3_CHANNEL_HUE] > 7){
        return 1;
      }
    }
  }
  return 0;
}

uint8_t estRouge(cc3_pixel_t pix){
  if(pix.channel[CC3_CHANNEL_SAT] > 45){
    if(pix.channel[CC3_CHANNEL_HUE] < 7){
      return 1;
    }
    if(pix.channel[CC3_CHANNEL_HUE] > 245){
	  return 1;
	}
  }
  return 0;
}
uint8_t estBleu(cc3_pixel_t pix){
  if(pix.channel[CC3_CHANNEL_SAT] > 45){
    if(pix.channel[CC3_CHANNEL_HUE] <= 200 || pix.channel[CC3_CHANNEL_HUE] > 175){
      return 1;
    }
  }
  return 0;
}
uint8_t estVert(cc3_pixel_t pix){
  if(pix.channel[CC3_CHANNEL_SAT] > 45){
    if(pix.channel[CC3_CHANNEL_HUE] <= 175 || pix.channel[CC3_CHANNEL_HUE] > 64){
      return 1;
    }
  }
  return 0;
}
uint8_t estNoir(cc3_pixel_t pix){
  if(pix.channel[CC3_CHANNEL_SAT] < 38){
      return 1;
  }
  return 0;
}

uint8_t estBlanc(cc3_pixel_t pix){
  // Pour éclairage artificiel
  if(pix.channel[CC3_CHANNEL_VAL] > 75){
  // Pour lumière naturelle
  //if(pix.channel[CC3_CHANNEL_VAL] > 140){
      return 1;
  }
  return 0;
}

inline void cc3_rgb2tls (cc3_pixel_t * pix)
{
  uint8_t hue, sat, val;
  uint8_t rgb_min, rgb_max;
  rgb_max = 0;
  rgb_min = 255;
  if (pix->channel[CC3_CHANNEL_RED] > rgb_max)
    rgb_max = pix->channel[CC3_CHANNEL_RED];
  if (pix->channel[CC3_CHANNEL_GREEN] > rgb_max)
    rgb_max = pix->channel[CC3_CHANNEL_GREEN];
  if (pix->channel[CC3_CHANNEL_BLUE] > rgb_max)
    rgb_max = pix->channel[CC3_CHANNEL_BLUE];
  if (pix->channel[CC3_CHANNEL_RED] < rgb_min)
    rgb_min = pix->channel[CC3_CHANNEL_RED];
  if (pix->channel[CC3_CHANNEL_GREEN] < rgb_min)
    rgb_min = pix->channel[CC3_CHANNEL_GREEN];
  if (pix->channel[CC3_CHANNEL_BLUE] < rgb_min)
    rgb_min = pix->channel[CC3_CHANNEL_BLUE];

// compute V
  val = (rgb_max/2) + (rgb_min/2);
  if (rgb_max == 0) {
    hue = sat = 0;
    pix->channel[CC3_CHANNEL_HUE] = 0;
    pix->channel[CC3_CHANNEL_SAT] = 0;
    pix->channel[CC3_CHANNEL_VAL] = val;
    return;
  }

// compute S
  sat = rgb_max - rgb_min;
  if (sat == 0) {
    pix->channel[CC3_CHANNEL_HUE] = 0;
    pix->channel[CC3_CHANNEL_SAT] = 0;
    pix->channel[CC3_CHANNEL_VAL] = val;
    return;
  }

// compute H
  if (rgb_max == pix->channel[CC3_CHANNEL_RED]) {
    hue =
      0 + 43 * (pix->channel[CC3_CHANNEL_GREEN] -
                pix->channel[CC3_CHANNEL_BLUE]) / (rgb_max - rgb_min);
  }
  else if (rgb_max == pix->channel[CC3_CHANNEL_GREEN]) {
    hue =
      85 + 43 * (pix->channel[CC3_CHANNEL_BLUE] -
                 pix->channel[CC3_CHANNEL_RED]) / (rgb_max - rgb_min);
  }
  else {                        /* rgb_max == blue */

    hue =
      171 + 43 * (pix->channel[CC3_CHANNEL_RED] -
                  pix->channel[CC3_CHANNEL_GREEN]) / (rgb_max - rgb_min);
  }
  pix->channel[CC3_CHANNEL_HUE] = hue;
  pix->channel[CC3_CHANNEL_SAT] = (rgb_max + rgb_min)/2;
  pix->channel[CC3_CHANNEL_VAL] = val;
}


