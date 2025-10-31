import gradio as gr
import subprocess
import os
import tempfile
import threading
import time
from pathlib import Path
import shutil

# Global variables for real-time log updates
current_process = None
log_content = []

def get_magnet_links():
	"""Read magnet links from the magnet_links.txt file"""
	magnet_file = Path(__file__).parent.parent / "magnet_links.txt"
	magnet_links = []
	
	if magnet_file.exists():
		with open(magnet_file, 'r') as f:
			for line in f:
				if ':' in line:
					name, link = line.strip().split(':', 1)
					magnet_links.append((name.strip(), link.strip()))
	
	return magnet_links

def get_torrent_files():
	"""Get list of .torrent files from project root"""
	project_root = Path(__file__).parent.parent
	torrent_files = list(project_root.glob("*.torrent"))
	return [str(f.name) for f in torrent_files]

def make_binary_executable():
	"""Ensure the bittorrent binary has execute permissions"""
	binary_path = Path(__file__).parent / "bittorrent"
	if binary_path.exists():
		os.chmod(binary_path, 0o755)
		return True
	return False

def run_command(cmd, cwd=None):
	"""Run a command and return the output"""
	try:
		result = subprocess.run(cmd, shell=True, capture_output=True, text=True, cwd=cwd)
		return result.returncode, result.stdout, result.stderr
	except Exception as e:
		return 1, "", str(e)

def get_torrent_info(torrent_file_path=None, magnet_link=None):
	"""Get info about torrent file or magnet link"""
	ui_dir = Path(__file__).parent
	
	if not make_binary_executable():
		return "❌ Error: BitTorrent binary not found or cannot be made executable"
	
	if torrent_file_path:
		# For torrent files
		cmd = f"./bittorrent info '{torrent_file_path}'"
	elif magnet_link:
		# For magnet links
		cmd = f"./bittorrent magnet_info '{magnet_link}'"
	else:
		return "❌ No torrent file or magnet link provided"
	
	returncode, stdout, stderr = run_command(cmd, cwd=ui_dir)
	
	if returncode == 0:
		cleaned_stdout = stdout.replace("codecrafters.", "")
		return f"ℹ️ Torrent Information:\n{cleaned_stdout}"
	else:
		cleaned_stderr = stderr.replace("codecrafters.", "")
		return f"❌ Error getting torrent info:\n{cleaned_stderr}"

def download_content(torrent_file_path=None, magnet_link=None, progress=gr.Progress()):
	"""Download content from torrent file or magnet link"""
	global current_process, log_content
	
	ui_dir = Path(__file__).parent
	downloaded_file = ui_dir / "downloaded_file.gif"  # Use absolute path

	# Remove existing download file if it exists
	if downloaded_file.exists():
		if downloaded_file.is_dir():
			shutil.rmtree(downloaded_file)
		else:
			downloaded_file.unlink()
	
	if not make_binary_executable():
		return "❌ Error: BitTorrent binary not found or cannot be made executable", "", None, False
	
	log_content = []
	
	# Prepare command
	if torrent_file_path:
		cmd = f"./bittorrent download -o {downloaded_file} '{torrent_file_path}'"
		log_content.append(f"📥 Starting download from torrent file: {os.path.basename(torrent_file_path)}")
	elif magnet_link:
		cmd = f"./bittorrent magnet_download -o {downloaded_file} '{magnet_link}'"
		log_content.append(f"🧲 Starting download from magnet link")
	else:
		return "❌ No torrent file or magnet link provided", "", None, False
	
	log_content.append(f"📁 Download path: {downloaded_file}")
	log_content.append(f"⚡ Command: {cmd.replace("codecrafters.", "")}")
	log_content.append("🚀 Starting download...")
	
	# Run download and wait for completion
	try:
		result = subprocess.run(
			cmd, 
			shell=True, 
			capture_output=True, 
			text=True, 
			cwd=ui_dir
		)
		
		log_content.append(f"📊 Process completed with exit code: {result.returncode}")
		
		# Add command output to logs
		if result.stdout:
			for line in result.stdout.split('\n'):
				cleaned_line = line.strip().replace("codecrafters.", "")
				log_content.append(cleaned_line)
	
		if result.stderr:
			for line in result.stderr.split('\n'):
				if line.strip():
					cleaned_line = line.strip().replace("codecrafters.", "")
					log_content.append(f"⚠️ {cleaned_line}")
		
		if result.returncode == 0:
			log_content.append("✅ Download completed successfully!")
			
			# Check if the downloaded file actually exists
			if downloaded_file.exists():
				log_content.append(f"📁 File found: {downloaded_file}")
				status = f"✅ Download completed! File: {downloaded_file.name}"
				return status, "\n".join(log_content), str(downloaded_file), True
			else:
				log_content.append(f"⚠️ Expected file not found at: {downloaded_file}")
				# Check if files were created in the current directory
				files_in_ui_dir = list(ui_dir.glob("*"))
				log_content.append(f"🔍 Files in UI directory: {[f.name for f in files_in_ui_dir if f.is_file()]}")
				return "⚠️ Download completed but expected file not found", "\n".join(log_content), None, False
		else:
			log_content.append(f"❌ Download failed with exit code: {result.returncode}")
			return f"❌ Download failed", "\n".join(log_content), None, False
			
	except Exception as e:
		log_content.append(f"❌ Error during download: {str(e)}")
		return f"❌ Error during download: {str(e)}", "\n".join(log_content), None, False

def get_current_logs():
	"""Get current logs for real-time updates"""
	global log_content
	return "\n".join(log_content) if log_content else "No logs yet..."

def handle_torrent_file_upload(file):
	"""Handle uploaded torrent file"""
	if file is None:
		return "No file uploaded", ""
	
	# Get info about the uploaded file
	info = get_torrent_info(torrent_file_path=file.name)
	return f"📁 Uploaded file: {os.path.basename(file.name)}", info

def handle_preset_torrent_selection(choice):
	"""Handle selection of preset torrent file"""
	if not choice or choice == "Select a torrent file...":
		return "", ""
	
	project_root = Path(__file__).parent.parent
	torrent_path = project_root / choice
	
	if torrent_path.exists():
		info = get_torrent_info(torrent_file_path=str(torrent_path))
		return f"📁 Selected file: {choice}", info
	else:
		return f"❌ Error: File {choice} not found", ""

def handle_magnet_link_selection(choice):
	"""Handle selection of preset magnet link"""
	if not choice or choice == "Select a magnet link...":
		return "", ""
	
	# Extract the magnet link from the choice
	magnet_links = get_magnet_links()
	selected_link = None
	
	for name, link in magnet_links:
		if choice.startswith(name):
			selected_link = link
			break
	
	if selected_link:
		info = get_torrent_info(magnet_link=selected_link)
		return f"🧲 Magnet-Link: {choice}", info
	else:
		return f"❌ Error: Magnet link not found", ""

def handle_custom_magnet_input(magnet_link):
	"""Handle custom magnet link input"""
	if not magnet_link.strip():
		return "", ""
	
	if not magnet_link.startswith("magnet:"):
		return "❌ Error: Invalid magnet link format", ""
	
	info = get_torrent_info(magnet_link=magnet_link)
	return f"🧲 Magnet link: ", info

def download_from_torrent_file(uploaded_file, preset_choice):
	"""Download from torrent file (uploaded or preset)"""
	if uploaded_file is not None:
		status, logs, file_path, show_file = download_content(torrent_file_path=uploaded_file.name)
		if file_path:
			return status, logs, str(file_path), gr.update(visible=show_file)
		else:
			return status, logs, None, gr.update(visible=False)
	elif preset_choice and preset_choice != "Select a torrent file...":
		project_root = Path(__file__).parent.parent
		torrent_path = project_root / preset_choice
		if torrent_path.exists():
			status, logs, file_path, show_file = download_content(torrent_file_path=str(torrent_path))
			if file_path:
				return status, logs, str(file_path), gr.update(visible=show_file)
			else:
				return status, logs, None, gr.update(visible=False)
		else:
			return f"❌ Error: File {preset_choice} not found", "", None, gr.update(visible=False)
	else:
		return "❌ No torrent file selected", "", None, gr.update(visible=False)

def download_from_magnet_link(custom_magnet, preset_magnet):
	"""Download from magnet link (custom or preset)"""
	magnet_link = None
	
	if custom_magnet.strip():
		magnet_link = custom_magnet.strip()
	elif preset_magnet and preset_magnet != "Select a magnet link...":
		# Extract the magnet link from the preset choice
		magnet_links = get_magnet_links()
		for name, link in magnet_links:
			if preset_magnet.startswith(name):
				magnet_link = link
				break
	
	if magnet_link:
		status, logs, file_path, show_file = download_content(magnet_link=magnet_link)
		if file_path:
			return status, logs, str(file_path), gr.update(visible=show_file)
		else:
			return status, logs, None, gr.update(visible=False)
	else:
		return "❌ No magnet link provided", "", None, gr.update(visible=False)

custom_css = """
.header-text {
	text-align: center;
	color: white !important;
	font-size: 2.5em !important;
	font-weight: bold !important;
}

.download-btn {
	min-height: 50px !important;
	font-size: 1.1em !important;
}

.log-box {
	background-color: #1e1e1e !important;
	color: #00ff00 !important;
}
"""


def create_ui():
	"""Create the Gradio interface"""
	
	with gr.Blocks(css=custom_css, title="BitTorrent Client", fill_width=True) as app:
		
		# Header
		gr.HTML("""
			<div class="header-text">
				🌟 BitTorrent Client 🌟<br>
				<small style="font-size: 0.6em; color: white;">Download from torrent files or magnet links</small>
			</div>
		""")
		
		with gr.Row():
			# Left Column - Input Controls
			with gr.Column(scale=1):
				gr.HTML("<h2>📥 Download Controls</h2>")
				
				# Magnet Link Section
				with gr.Group():
					gr.HTML("<h3>🧲 Magnet Links</h3>")
					
					magnet_links = get_magnet_links()
					magnet_choices = ["Select a magnet link..."] + [f"{name} - {link[:50]}..." for name, link in magnet_links]
					
					preset_magnet = gr.Dropdown(
						choices=magnet_choices,
						value="Select a magnet link...",
						label="📋 Preset Magnet Links"
					)
					
					custom_magnet = gr.Textbox(
						label="✏️ Custom Magnet Link",
						placeholder="magnet:?xt=urn:btih:..."
					)
				
				# Torrent File Section
				with gr.Group():
					gr.HTML("<h3>📁 Torrent Files</h3>")
					
					torrent_files = get_torrent_files()
					torrent_choices = ["Select a torrent file..."] + torrent_files
					
					preset_torrent = gr.Dropdown(
						choices=torrent_choices,
						value="Select a torrent file...",
						label="📋 Preset Torrent Files"
					)
					
					uploaded_torrent = gr.File(
						label="📤 Upload Torrent File",
						file_types=[".torrent"]
					)
				
				# Download Buttons
				with gr.Row():
					download_torrent_btn = gr.Button(
						"📥 Download from Torrent", 
						variant="primary",
						elem_classes=["download-btn"]
					)
					download_magnet_btn = gr.Button(
						"🧲 Download from Magnet", 
						variant="primary",
						elem_classes=["download-btn"]
					)
				
				with gr.Row():
					# Downloaded File
					downloaded_file = gr.File(
						label="💾 Downloaded File",
						visible=False
					)
			
			# Right Column - Information and Logs
			with gr.Column(scale=1):
				gr.HTML("<h2>📊 Information & Logs</h2>")
				
				# Info Display
				with gr.Group():
					gr.HTML("<h3>ℹ️ Torrent Information</h3>")
					
					file_status = gr.Textbox(
						label="📄 File Status",
						interactive=False,
						elem_classes=["info-box"]
					)
					
					info_display = gr.Textbox(
						label="📋 Detailed Information",
						lines=10,
						interactive=False,
						elem_classes=["info-box"]
					)
				
				# Logs Display
				with gr.Group():
					gr.HTML("<h3>📝 Download Logs</h3>")
					
					download_status = gr.Textbox(
						label="📊 Download Status",
						interactive=False
					)
					
					logs_display = gr.Textbox(
						label="🖥️ Real-time Logs",
						lines=8,
						interactive=False,
						elem_classes=["log-box"]
					)
				
		
		# Event Handlers
		
		# File upload handler
		uploaded_torrent.change(
			fn=handle_torrent_file_upload,
			inputs=[uploaded_torrent],
			outputs=[file_status, info_display]
		)
		
		# Preset torrent selection handler
		preset_torrent.change(
			fn=handle_preset_torrent_selection,
			inputs=[preset_torrent],
			outputs=[file_status, info_display]
		)
		
		# Preset magnet selection handler
		preset_magnet.change(
			fn=handle_magnet_link_selection,
			inputs=[preset_magnet],
			outputs=[file_status, info_display]
		)
		
		# Custom magnet input handler
		custom_magnet.change(
			fn=handle_custom_magnet_input,
			inputs=[custom_magnet],
			outputs=[file_status, info_display]
		)
		
		# Download button handlers
		download_torrent_btn.click(
			fn=download_from_torrent_file,
			inputs=[uploaded_torrent, preset_torrent],
			outputs=[download_status, logs_display, downloaded_file, downloaded_file]
		)
		
		download_magnet_btn.click(
			fn=download_from_magnet_link,
			inputs=[custom_magnet, preset_magnet],
			outputs=[download_status, logs_display, downloaded_file, downloaded_file]
		)
	
	return app

if __name__ == "__main__":
	# Ensure binary is executable
	make_binary_executable()
	
	# Create and launch the app
	app = create_ui()
	app.launch(
		server_name="0.0.0.0",
		server_port=7860,
		share=False,
		show_error=True,
		quiet=False
	)
