#include "Platformer.h"

#include <SDL3/SDL.h>

#include "../engine/Scene.h"
#include "../engine/GameObject.h"
#include "../engine/Component.h"
#include "../engine/Transform.h"
#include "../engine/SpriteRenderer.h"
#include "../engine/SpriteAnimator.h"
#include "../engine/RigidBody2D.h"
#include "../engine/BoxCollider.h"
#include "../engine/TilemapRenderer.h"
#include "../engine/Camera.h"
#include "../engine/FollowCamera.h"

// Izquierda/derecha + salto con espacio. Detecta flanco de tecla con memoria
// del frame anterior para que el salto no se dispare multiples veces.
class PlatformerController : public Component {
public:
    float speed = 250.0f, jump = 650.0f;
    float dashSpeed = 2500.0f;
    float dashDuration = 0.15f;
    float dashCooldownTime = 0.6f;
    float gravityDirection = 1.0f;

    // Variables para registrar el punto de aparición (Respawn)
    float startX = 0.0f;
    float startY = 0.0f;
    bool firstFrame = true;

    float timeElapsed = 0.0f;
    bool hasWon = false;

    void update(float dt) override {
        // En el primer frame guardamos la posición segura de inicio
        if (firstFrame) {
            startX = gameObject->transform->x;
            startY = gameObject->transform->y;
            firstFrame = false;
        }
        if (!hasWon) {
            timeElapsed += dt;
        }
       
     
        int minutes = (int)(timeElapsed / 60.0f);
        int seconds = (int)(timeElapsed) % 60;
        int milliseconds = (int)((timeElapsed - std::floor(timeElapsed)) * 100.0f);

        // Obtenemos el renderer de la escena para llegar a la ventana de SDL3
        SDL_Renderer* renderer = gameObject->scene->getRenderer();
        if (renderer) {
            SDL_Window* window = SDL_GetRenderWindow(renderer);
            if (window) {
                char titleBuffer[128]; // Ampliamos el tamaño por si acaso

                if (hasWon) {
                    
                    SDL_snprintf(titleBuffer, sizeof(titleBuffer), "GravityDash - GANASTE! - Tiempo Record: %02d:%02d:%02d", minutes, seconds, milliseconds);
                }
                else {
                   
                    SDL_snprintf(titleBuffer, sizeof(titleBuffer), "GravityDash - Tiempo: %02d:%02d:%02d", minutes, seconds, milliseconds);
                }

                SDL_SetWindowTitle(window, titleBuffer);
            }
        }


        const bool* keys = SDL_GetKeyboardState(nullptr);
        auto rb = gameObject->getComponent<RigidBody2D>();
        auto sprite = gameObject->getComponent<SpriteRenderer>();
        auto anim = gameObject->getComponent<SpriteAnimator>();

        // 1. DASH
        if (dashCooldownTimer > 0.0f) {
            dashCooldownTimer -= dt;
        }

        if (dashTimer > 0.0f) {
            dashTimer -= dt;
            if (rb) {
                rb->velocityX = dashDirection * dashSpeed;
                rb->velocityY = 0.0f;
            }
            if (anim) {
                anim->play("run");
            }

            dashPrev = keys[SDL_SCANCODE_LSHIFT];
            jumpPrev = keys[SDL_SCANCODE_X];
            return;
        }

        // 2. MOVIMIENTO
        float moveX = 0.0f;
        if (keys[SDL_SCANCODE_LEFT])  moveX -= 1.0f;
        if (keys[SDL_SCANCODE_RIGHT]) moveX += 1.0f;
        if (rb) rb->velocityX = moveX * speed;

        // 3. CAMBIO DE GRAVEDAD SIMÉTRICO
        bool gravityKey = keys[SDL_SCANCODE_C];
        if (gravityKey && !gravityPrev) {
            gravityDirection *= -1.0f;
            if (rb) {
                rb->gravityScale = gravityDirection;
            }
            coyote = 0.0f;
        }
        gravityPrev = gravityKey;

        // 4. TRIGGER DASH
        bool dashNow = keys[SDL_SCANCODE_Z];
        if (dashNow && !dashPrev && dashCooldownTimer <= 0.0f) {
            dashTimer = dashDuration;
            dashCooldownTimer = dashCooldownTime;

            if (moveX != 0.0f) {
                dashDirection = moveX;
            }
            else {
                dashDirection = (sprite && sprite->flipX) ? -1.0f : 1.0f;
            }
        }
        dashPrev = dashNow;

        // 5. COYOTE TIME NATIVO
        if (rb && rb->grounded) {
            coyote = coyoteTime;
        }
        else if (coyote > 0.0f) {
            coyote -= dt;
        }

        // 6. SALTO
        bool jumpNow = keys[SDL_SCANCODE_X];
        if (rb && jumpNow && !jumpPrev && coyote > 0.0f) {
            rb->velocityY = -jump * gravityDirection;
            coyote = 0.0f;
        }
        jumpPrev = jumpNow;

        // 7. VISUALES (FLIP)
        if (sprite) {
            if (moveX < 0) sprite->flipX = true;
            else if (moveX > 0) sprite->flipX = false;
            sprite->flipY = (gravityDirection < 0.0f);
        }

        // 8. ANIMACIONES
        if (anim && rb) {
            bool onGround = coyote > 0.0f;
            if (onGround) {
                anim->play(moveX != 0.0f ? "run" : "idle");
            }
            else {
                float relativeVerticalVelocity = rb->velocityY * gravityDirection;
                anim->play(relativeVerticalVelocity < 0.0f ? "jump" : "fall");
            }
        }
    }

    // 9. GATILLO DE EVENTO AUTOMÁTICO (Invocado por el notifyCollision de tu GameObject)
    void onCollision(GameObject* other) override {
        // Si chocamos con una entidad nombrada "Hazard" por el TilemapRenderer... morimos
        if (other && other->name == "Hazard") {
            reiniciarNivel();
        }
        //dash por bloque dorado
        if (other && other->name == "Dash") {
            auto sprite = gameObject->getComponent<SpriteRenderer>();

            
            float direccionImpulso = (sprite && sprite->flipX) ? -1.0f : 1.0f;

            
            impulsarPorBloque(direccionImpulso, dashSpeed);
        }
        if (other && other->name == "Winner") {
            if (!hasWon) {
                SDL_Log("¡Nivel Completado! Has tocado el bloque Winner.");
                hasWon = true; // Activa el freno de tiempo y cambia el título
            }
            
        }
    }
    void impulsarPorBloque(float direccionX, float velocidadPersonalizada = 2500.0f) {
        auto rb = gameObject->getComponent<RigidBody2D>();

        dashTimer = dashDuration;
        dashCooldownTimer = dashCooldownTime;
        dashDirection = direccionX;

        if (rb) {
            rb->velocityX = dashDirection * velocidadPersonalizada;
            rb->velocityY = 0.0f; 
        }
    }
    void reiniciarNivel() {
        auto rb = gameObject->getComponent<RigidBody2D>();
        

        // Regresar al checkpoint seguro
        gameObject->transform->x = startX;
        gameObject->transform->y = startY;

        // Drenar velocidades remanentes
        if (rb) {
            rb->velocityX = 0.0f;
            rb->velocityY = 0.0f;

            // Reestablecer gravedad por defecto al reaparecer
            gravityDirection = 1.0f;
            rb->gravityScale = 1.0f;
        }

        coyote = 0.0f;
        dashTimer = 0.0f;
        dashCooldownTimer = 0.0f;
        timeElapsed = 0.0f;
    }

private:
    bool  jumpPrev = false;
    bool dashPrev = false;
    float dashTimer = 0.0f;
    float dashCooldownTimer = 0.0f;
    float dashDirection = 1.0f;
    bool gravityPrev = false;
    float coyote = 0.0f;
    static constexpr float coyoteTime = 0.1f;
};

void buildPlatformer(Scene& scene) {
    GameObject* player = scene.createGameObject("Player");
    player->transform->y = 100.0f;
    player->transform->scaleX = player->transform->scaleY = 4.0f;
    // Sin textura inicial: cada animacion del Mask Dude es un .png aparte (una tira
    // horizontal de frames de 32x32), y el animator decide cual dibujar segun el estado.
    player->addComponent<SpriteRenderer>();
    auto anim = player->addComponent<SpriteAnimator>(32, 32, 1);
    const std::string mask = "assets/pixel_adventure/Main Characters/Virtual Guy/";
    anim->addStripAnimation("idle", mask + "Idle (32x32).png", 32, 32, 20.0f);
    anim->addStripAnimation("run",  mask + "Run (32x32).png",  32, 32, 20.0f);
    anim->addStripAnimation("jump", mask + "Jump (32x32).png", 32, 32, 20.0f);
    anim->addStripAnimation("fall", mask + "Fall (32x32).png", 32, 32, 20.0f);
    anim->play("idle");
    player->addComponent<RigidBody2D>(); // con gravedad
    auto col = player->addComponent<BoxCollider>();
    col->width = 64.0f; col->height = 110.0f; col->offsetY = 8.0f; // ajustado al cuerpo
    player->addComponent<PlatformerController>();

    // Suelo y plataformas con un TilemapRenderer real (reemplaza el cuadrado estirado).
    // El mapa se carga desde un archivo de texto (contenido del juego, en assets/);
    // se puede editar a mano sin recompilar. El tileset, tile, columnas y tiles solidos
    // van en la cabecera del .map. Ver assets/maps/level1.map.
    GameObject* tilemap = scene.createGameObject("Tilemap");
    // El Transform marca el ORIGEN del mapa (esquina superior izquierda de la celda 0,0).
    tilemap->transform->x = -960.0f;
    tilemap->transform->y = -262.0f; // colocado para que el suelo quede en pantalla (~y=250)
    tilemap->transform->scaleX = tilemap->transform->scaleY = 4.0f; // 16px -> 64px por celda
    auto tm = tilemap->addComponent<TilemapRenderer>(); // modo archivo: el tileset lo da el mapa
    // Nivel exportado desde Tiled (JSON, capa de tiles + tileset embebido). El .json
    // se espera en assets/maps/ y su "image" (relativa al .json) debe apuntar al tileset
    // accesible desde ahi (p.ej. ../pixel_adventure/Terrain/...). Los tiles solidos se
    // marcan en Tiled con una propiedad booleana "solid"=true en el tileset.
    if (!tm->loadFromTiledJson("assets/maps/platformer_level1.json"))
        SDL_Log("buildPlatformer: no se pudo cargar assets/maps/level1.json");
    // Alternativa: nuestro formato .map propio (queda como referencia).
    // if (!tm->loadFromFile("assets/maps/level1.map"))
    //     SDL_Log("buildPlatformer: no se pudo cargar assets/maps/level1.map");

    GameObject* cam = scene.createGameObject("MainCamera");
    cam->addComponent<Camera>();
    auto f = cam->addComponent<FollowCamera>();
    f->setTarget(player);
    f->deadZoneWidth = 200.0f; f->deadZoneHeight = 200.0f;
}
