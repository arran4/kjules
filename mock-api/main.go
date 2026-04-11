package main

import (
	"encoding/json"
	"fmt"
	"log"
	"math/rand"
	"net/http"
	"strconv"
	"strings"
	"sync"
	"time"
)

type Source struct {
	Name        string `json:"name"`
	DisplayName string `json:"displayName"`
	Description string `json:"description"`
}

type Session struct {
	Name                string `json:"name"`
	Id                  string `json:"id"`
	Prompt              string `json:"prompt"`
	State               string `json:"state"`
	CreateTime          string `json:"createTime"`
	UpdateTime          string `json:"updateTime"`
	Source              string `json:"source"`
	AutomationMode      string `json:"automationMode,omitempty"`
	RequirePlanApproval bool   `json:"requirePlanApproval,omitempty"`
}

type Activity struct {
	Name        string `json:"name"`
	Description string `json:"description"`
	CreateTime  string `json:"createTime"`
	State       string `json:"state"`
}

var (
	mu         sync.Mutex
	sources    []Source
	sessions   []Session
	activities map[string][]Activity
)

func initData() {
	sources = []Source{
		{Name: "sources/github/kde/kjules", DisplayName: "KDE kJules", Description: "KDE Jules API Client"},
		{Name: "sources/github/kde/kio", DisplayName: "KIO", Description: "Network transparent access to files and data"},
		{Name: "sources/github/qt/qtbase", DisplayName: "Qt Base", Description: "Qt Base module"},
	}

	for i := 0; i < 20; i++ {
		sources = append(sources, Source{
			Name:        fmt.Sprintf("sources/github/user/repo%d", i),
			DisplayName: fmt.Sprintf("User Repo %d", i),
			Description: "Mock repository for testing pagination.",
		})
	}

	activities = make(map[string][]Activity)

	states := []string{"PENDING", "RUNNING", "COMPLETED", "FAILED", "NEEDS_ATTENTION"}
	prompts := []string{
		"Fix the bug in the main window where it crashes on startup",
		"Add a new mock API to the application",
		"Update documentation for the new feature",
		"Refactor the API manager to use a base URL",
		"Implement pagination for the sessions view",
		"Optimize the rendering of list items",
		"Migrate the database to PostgreSQL",
		"Write unit tests for the core logic",
		"Create a new dark mode theme",
		"Integrate with the CI/CD pipeline",
	}

	now := time.Now()
	for i := 0; i < 50; i++ {
		state := states[rand.Intn(len(states))]
		id := fmt.Sprintf("session-%d", i)
		name := "sessions/" + id
		createTime := now.Add(-time.Duration(rand.Intn(100)) * time.Hour).Format(time.RFC3339)

		session := Session{
			Name:       name,
			Id:         id,
			Prompt:     prompts[rand.Intn(len(prompts))],
			State:      state,
			CreateTime: createTime,
			UpdateTime: createTime,
			Source:     sources[rand.Intn(len(sources))].Name,
		}
		sessions = append(sessions, session)

		numActs := rand.Intn(10) + 1
		var acts []Activity
		for j := 0; j < numActs; j++ {
			acts = append(acts, Activity{
				Name:        fmt.Sprintf("%s/activities/%d", name, j),
				Description: fmt.Sprintf("Activity %d for session %s", j, id),
				CreateTime:  createTime,
				State:       "COMPLETED",
			})
		}
		activities[name] = acts
	}

	go stateTransitionLoop()
}

func stateTransitionLoop() {
	for {
		time.Sleep(5 * time.Second)
		mu.Lock()
		for i := range sessions {
			if sessions[i].State == "PENDING" {
				sessions[i].State = "RUNNING"
				sessions[i].UpdateTime = time.Now().Format(time.RFC3339)

				acts := activities[sessions[i].Name]
				acts = append(acts, Activity{
					Name:        fmt.Sprintf("%s/activities/%d", sessions[i].Name, len(acts)),
					Description: "Started running the session",
					CreateTime:  time.Now().Format(time.RFC3339),
					State:       "COMPLETED",
				})
				activities[sessions[i].Name] = acts
			} else if sessions[i].State == "RUNNING" {
				if rand.Float32() > 0.5 {
					sessions[i].State = "COMPLETED"
					sessions[i].UpdateTime = time.Now().Format(time.RFC3339)

					acts := activities[sessions[i].Name]
					acts = append(acts, Activity{
						Name:        fmt.Sprintf("%s/activities/%d", sessions[i].Name, len(acts)),
						Description: "Session completed successfully",
						CreateTime:  time.Now().Format(time.RFC3339),
						State:       "COMPLETED",
					})
					activities[sessions[i].Name] = acts
				}
			}
		}
		mu.Unlock()
	}
}

func main() {
	initData()

	http.HandleFunc("/v1alpha/sources", func(w http.ResponseWriter, r *http.Request) {
		mu.Lock()
		defer mu.Unlock()

		pageToken := r.URL.Query().Get("pageToken")
		pageSize := 10
		startIdx := 0
		if pageToken != "" {
			var err error
			startIdx, err = strconv.Atoi(pageToken)
			if err != nil {
				http.Error(w, "Invalid pageToken", http.StatusBadRequest)
				return
			}
		}

		if startIdx > len(sources) {
			startIdx = len(sources)
		}
		endIdx := startIdx + pageSize
		nextPageToken := ""
		if endIdx < len(sources) {
			nextPageToken = strconv.Itoa(endIdx)
		} else {
			endIdx = len(sources)
		}

		resp := map[string]interface{}{
			"sources": sources[startIdx:endIdx],
		}
		if nextPageToken != "" {
			resp["nextPageToken"] = nextPageToken
		}

		w.Header().Set("Content-Type", "application/json")
		json.NewEncoder(w).Encode(resp)
	})

	http.HandleFunc("/v1alpha/sessions", func(w http.ResponseWriter, r *http.Request) {
		mu.Lock()
		defer mu.Unlock()

		if r.Method == "POST" {
			var req map[string]interface{}
			if err := json.NewDecoder(r.Body).Decode(&req); err != nil {
				http.Error(w, err.Error(), http.StatusBadRequest)
				return
			}

			id := fmt.Sprintf("session-%d", len(sessions)+1000)
			name := "sessions/" + id
			now := time.Now().Format(time.RFC3339)

			prompt := ""
			if p, ok := req["prompt"].(string); ok {
				prompt = p
			}

			session := Session{
				Name:       name,
				Id:         id,
				Prompt:     prompt,
				State:      "PENDING",
				CreateTime: now,
				UpdateTime: now,
			}
			if src, ok := req["source"].(string); ok {
				session.Source = src
			}

			sessions = append([]Session{session}, sessions...)
			activities[name] = []Activity{
				{
					Name:        name + "/activities/0",
					Description: "Session created",
					CreateTime:  now,
					State:       "COMPLETED",
				},
			}

			w.Header().Set("Content-Type", "application/json")
			json.NewEncoder(w).Encode(session)
			return
		}

		pageToken := r.URL.Query().Get("pageToken")
		pageSize := 15
		startIdx := 0
		if pageToken != "" {
			var err error
			startIdx, err = strconv.Atoi(pageToken)
			if err != nil {
				http.Error(w, "Invalid pageToken", http.StatusBadRequest)
				return
			}
		}

		if startIdx > len(sessions) {
			startIdx = len(sessions)
		}
		endIdx := startIdx + pageSize
		nextPageToken := ""
		if endIdx < len(sessions) {
			nextPageToken = strconv.Itoa(endIdx)
		} else {
			endIdx = len(sessions)
		}

		resp := map[string]interface{}{
			"sessions": sessions[startIdx:endIdx],
		}
		if nextPageToken != "" {
			resp["nextPageToken"] = nextPageToken
		}

		w.Header().Set("Content-Type", "application/json")
		json.NewEncoder(w).Encode(resp)
	})

	http.HandleFunc("/v1alpha/sessions/", func(w http.ResponseWriter, r *http.Request) {
		mu.Lock()
		defer mu.Unlock()

		path := strings.TrimPrefix(r.URL.Path, "/v1alpha/")
		parts := strings.Split(path, "/")

		if len(parts) == 2 {
			name := "sessions/" + parts[1]
			for _, s := range sessions {
				if s.Name == name || s.Id == parts[1] {
					w.Header().Set("Content-Type", "application/json")
					json.NewEncoder(w).Encode(s)
					return
				}
			}
			http.NotFound(w, r)
			return
		} else if len(parts) == 3 && parts[2] == "activities" {
			name := "sessions/" + parts[1]
			acts, ok := activities[name]
			if !ok {
				acts = []Activity{}
			}
			w.Header().Set("Content-Type", "application/json")
			json.NewEncoder(w).Encode(map[string]interface{}{
				"activities": acts,
			})
			return
		}

		http.NotFound(w, r)
	})

	log.Println("Mock API listening on :8080")
	log.Fatal(http.ListenAndServe(":8080", nil))
}
