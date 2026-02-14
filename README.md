# Paper Vendo Machine Capstone Project

This repository contains the complete system for the Paper Vendo Machine, including the web-based management system, API, and hardware controller code.

## 🚀 How to Run the Website

Follow these steps to set up and run the system on your local machine using XAMPP.

### 1. Prerequisites
- Install **XAMPP** (includes Apache and MySQL).
- Ensure your project folder is located in `C:\xampp\htdocs\`.

### 2. Move Project to htdocs
Make sure the folder name is `Paper Vendo Machine Capstone` and it is placed inside:
`C:\xampp\htdocs\Paper Vendo Machine Capstone`

### 3. Start XAMPP Control Panel
1. Open the **XAMPP Control Panel**.
2. Click **Start** for **Apache**.
3. Click **Start** for **MySQL**.

### 4. Set Up the Database
1. Open your browser and go to: `http://localhost/phpmyadmin/`
2. Click on **New** in the left sidebar.
3. Database Name: `paper_vendo_db`
4. Click **Create**.
5. Select the newly created `paper_vendo_db`.
6. Click the **Import** tab at the top.
7. Click **Choose File** and navigate to:
   `C:\xampp\htdocs\Paper Vendo Machine Capstone\Paper Vendo Website System\paper_vendo.sql`
8. Scroll down and click **Import**.

### 5. Access the System
Once the database is imported, you can access the website at:
**[http://localhost/Paper Vendo Machine Capstone/Paper Vendo Website System](http://localhost/Paper Vendo Machine Capstone/Paper Vendo Website System)**

### 🔑 Default Credentials
- **Username:** `admin`
- **Password:** `admin123`

---

## 📂 Project Structure
- **/Paper Vendo Website System** - The main web dashboard for monitoring and management.
- **/Api Folder** - Backend API for communication between the hardware and the website.
- **/Arduino code stuff folder** - Firmware for the Arduino controller.
- **/Esp32 code stuff folder** - Firmware for the ESP32 (internet connectivity).

## 🛠️ Built With
- **Frontend/Backend:** HTML, CSS, PHP, JavaScript
- **Database:** MySQL
- **Hardware:** Arduino, ESP32
